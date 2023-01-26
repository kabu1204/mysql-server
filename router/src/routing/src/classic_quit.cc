/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "classic_quit.h"

#include "classic_connection_base.h"
#include "classic_forwarder.h"
#include "classic_frame.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/connection_pool_component.h"
#include "tracer.h"

/**
 * forward the list-fields message flow.
 *
 * Expected overall flow:
 *
 * @code
 * c->r: COM_QUIT
 * alt can not be pooled
 *    r->s: COM_QUIT
 * else
 *    r->s: (add to pool)
 * end
 * c<-r: (close)
 * @endcode
 *
 * It is no error, if there is no server-connection.
 */
stdx::expected<Processor::Result, std::error_code> QuitProcessor::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::ClientShutdown:
      return client_shutdown();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

static PooledClassicConnection make_pooled_connection(
    TlsSwitchableConnection &&other) {
  auto *classic_protocol_state =
      dynamic_cast<ClassicProtocolState *>(other.protocol());
  return {std::move(other.connection()),
          other.channel()->release_ssl(),
          classic_protocol_state->server_capabilities(),
          classic_protocol_state->client_capabilities(),
          classic_protocol_state->server_greeting(),
          other.ssl_mode(),
          classic_protocol_state->username(),
          classic_protocol_state->schema(),
          classic_protocol_state->attributes()};
}

static TlsSwitchableConnection make_connection_from_pooled(
    PooledClassicConnection &&other) {
  return {std::move(other.connection()),
          nullptr,  // routing_conn
          other.ssl_mode(), std::make_unique<Channel>(std::move(other.ssl())),
          std::make_unique<ClassicProtocolState>(
              other.server_capabilities(), other.client_capabilities(),
              other.server_greeting(), other.username(), other.schema(),
              other.attributes())};
}

stdx::expected<Processor::Result, std::error_code> QuitProcessor::command() {
  auto socket_splicer = connection()->socket_splicer();
  auto src_protocol = connection()->client_protocol();
  auto src_channel = socket_splicer->client_channel();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::client::Quit>(
          src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("quit::command"));
  }

  {
    // if the client already shutdown the socket, close our side of it too.
    //
    // releasing it early releases the socket faster then waiting for the
    // RecvClient later in ::client_shutdown()
    std::array<uint8_t, 1> buf{};
    auto recv_res = net::impl::socket::recv(
        socket_splicer->client_conn().native_handle(), buf.data(), buf.size(),
        net::socket_base::message_peek);
    if (recv_res && recv_res.value() == 0) {
      // client already closed the socket.
      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event()
                     .stage("close::client")
                     .direction(Tracer::Event::Direction::kClientClose));
      }
      (void)socket_splicer->client_conn().close();
    }
  }

  if (!socket_splicer->server_conn().is_open()) {
    discard_current_msg(src_channel, src_protocol);

    stage(Stage::ClientShutdown);
    return Result::Again;
  }

  // move the connection to the pool.
  //
  // the pool will either close it or keep it alive.
  auto &pools = ConnectionPoolComponent::get_instance();

  if (auto pool = pools.get(ConnectionPoolComponent::default_pool_name())) {
    auto is_full_res =
        pool->add_if_not_full(make_pooled_connection(std::exchange(
            socket_splicer->server_conn(),
            TlsSwitchableConnection{
                nullptr, nullptr, socket_splicer->server_conn().ssl_mode(),
                std::make_unique<ClassicProtocolState>()})));

    if (!is_full_res) {
      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("quit::pooled"));
      }

      // the connect was pooled, discard the Quit message.
      discard_current_msg(src_channel, src_protocol);

      stage(Stage::ClientShutdown);
      return Result::Again;
    }

    socket_splicer->server_conn() =
        make_connection_from_pooled(std::move(*is_full_res));
  }

  stage(Stage::ClientShutdown);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
QuitProcessor::client_shutdown() {
  auto socket_splicer = connection()->socket_splicer();

  stage(Stage::Done);

  if (!socket_splicer->client_conn().is_open()) return Result::Again;

  // client's expect the server to close first.
  //
  // close the sending side and wait until the client closed its side too.
  (void)socket_splicer->client_conn().shutdown(net::socket_base::shutdown_send);

  // wait for the client to send data ... which should be a connection close.
  return Result::RecvFromClient;
}

// sender

stdx::expected<Processor::Result, std::error_code> QuitSender::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::CloseSocket:
      return close_socket();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> QuitSender::command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *dst_protocol = connection()->server_protocol();
  auto *dst_channel = socket_splicer->server_channel();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("quit::command"));
  }

  dst_protocol->seq_id(0xff);

  auto msg_res =
      ClassicFrame::send_msg<classic_protocol::borrowed::message::client::Quit>(
          dst_channel, dst_protocol, {});
  if (!msg_res) return send_server_failed(msg_res.error());

  stage(Stage::CloseSocket);
  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> QuitSender::close_socket() {
  auto *socket_splicer = connection()->socket_splicer();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event()
                 .stage("quit::close")
                 .direction(Tracer::Event::Direction::kServerClose));
  }

  (void)socket_splicer->server_conn().close();

  stage(Stage::Done);
  return Result::Again;
}

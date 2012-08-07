/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
*/

var mysqlconnection = require("./MysqlConnectionPool.js");

var MysqlDefaultConnectionProperties = {  
  "implementation" : "mysql",
  "database"       : "test",
  
  "mysql_host"     : "localhost",
  "mysql_port"     : 3306,
  "mysql_user"     : "",
  "mysql_password" : "",
  "mysql_socket"   : null,
  "debug"          : true,
  "mysql_debug"    : false
};


exports.getDefaultConnectionProperties = function() {
  return MysqlDefaultConnectionProperties;
};


exports.connectSync = function(properties) {
  var connectionPool = new mysqlconnection.DBConnectionPool(properties);
  connectionPool.connectSync();
  return connectionPool;
};


exports.getFactoryKey = function(properties) {
  var socket = properties.mysql_socket;
  if (!socket) socket = properties.mysql_host + ':' + properties.mysql_port;
  // TODO: hash user and password to avoid security issue
  var key = properties.implementation + "://" + socket + 
    "+" + properties.mysql_user + "<" + properties.mysql_password + ">";
//  console.log(key);
  return key;
};


exports.connect = function(properties, sessionFactory_callback) {
  //the caller of this function is the session factory
  var callback = sessionFactory_callback;
  // create the connection pool from the properties
  var connectionPool = new mysqlconnection.DBConnectionPool(properties);
  // connect to the database
  connectionPool.connect(function(err, connection) {
    callback(err, connectionPool);
  });
};


exports.getNativeCodeModules = function() {
  return [];
}

/*
 * Copyright (c) 2014, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// **NOTE:** This file is a snapshot of the WebSocket++ utility client tutorial.
// Additional related material can be found in the tutorials/utility_client
// directory of the WebSocket++ repository.

#include "WebSocketManager.h"

/// TLS Initialization handler
/**
 * WebSocket++ core and the Asio Transport do not handle TLS context creation
 * and setup. This callback is provided so that the end user can set up their
 * TLS context using whatever settings make sense for their application.
 *
 * As Asio and OpenSSL do not provide great documentation for the very common
 * case of connect and actually perform basic verification of server certs this
 * example includes a basic implementation (using Asio and OpenSSL) of the
 * following reasonable default settings and verification steps:
 *
 * - Disable SSLv2 and SSLv3
 * - Load trusted CA certificates and verify the server cert is trusted.
 * - Verify that the hostname matches either the common name or one of the
 *   subject alternative names on the certificate.
 *
 * This is not meant to be an exhaustive reference implimentation of a perfect
 * TLS client, but rather a reasonable starting point for building a secure
 * TLS encrypted WebSocket client.
 *
 * If any TLS, Asio, or OpenSSL experts feel that these settings are poor
 * defaults or there are critically missing steps please open a GitHub issue
 * or drop a line on the project mailing list.
 *
 * Note the bundled CA cert ca-chain.cert.pem is the CA cert that signed the
 * cert bundled with echo_server_tls. You can use print_client_tls with this
 * CA cert to connect to echo_server_tls as long as you use /etc/hosts or
 * something equivilent to spoof one of the names on that cert 
 * (websocketpp.org, for example).
 */
//context_ptr on_tls_init(const char * hostname, websocketpp::connection_hdl) {
context_ptr on_tls_init() {
    context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::no_sslv3 |
                         boost::asio::ssl::context::single_dh_use);
	//std::cout << "on_tls_init\n";

        //ctx->set_verify_mode(boost::asio::ssl::verify_peer);
        //ctx->set_verify_callback(bind(&verify_certificate, hostname, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));

        // Here we load the CA certificates of all CA's that this client trusts.
        //ctx->load_verify_file("ca-chain.cert.pem");
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
    return ctx;
}

WebSocketManager::~WebSocketManager()
{
  m_endpoint.stop_perpetual();

  for (con_list::const_iterator it = m_connection_list.begin(); it != m_connection_list.end(); ++it) {
    if (it->second->get_status() != "Open") {
      // Only close open connections
      continue;
    }

    std::cout << "> Closing connection " << it->second->get_id() << std::endl;

    websocketpp::lib::error_code ec;
    m_endpoint.close(it->second->get_hdl(), websocketpp::close::status::going_away, "", ec);
    if (ec) {
      std::cout << "> Error closing connection " << it->second->get_id() << ": "  
	<< ec.message() << std::endl;
    }
  }

  m_thread->join();
}

int WebSocketManager::Connect(const char* uri, client::connection_type::message_handler mh)
{
  websocketpp::lib::error_code ec;

  client::connection_ptr con = m_endpoint.get_connection(uri, ec);

  if (ec) {
    std::cout << "> Connect initialization error: " << ec.message() << std::endl;
    return -1;
  }

  int new_id = m_next_id++;
  connection_metadata::ptr metadata_ptr = websocketpp::lib::make_shared<connection_metadata>(new_id, con->get_handle(), uri);
  m_connection_list[new_id] = metadata_ptr;

  con->set_open_handler(websocketpp::lib::bind(
	&connection_metadata::on_open,
	metadata_ptr,
	&m_endpoint,
	websocketpp::lib::placeholders::_1
	));
  con->set_fail_handler(websocketpp::lib::bind(
	&connection_metadata::on_fail,
	metadata_ptr,
	&m_endpoint,
	websocketpp::lib::placeholders::_1
	));
  con->set_close_handler(websocketpp::lib::bind(
	&connection_metadata::on_close,
	metadata_ptr,
	&m_endpoint,
	websocketpp::lib::placeholders::_1
	));
  if(!mh) con->set_message_handler(websocketpp::lib::bind(
	&connection_metadata::on_message,
	metadata_ptr,
	websocketpp::lib::placeholders::_1,
	websocketpp::lib::placeholders::_2
	));
  else con->set_message_handler(mh);

  m_endpoint.connect(con);

  return new_id;
}

void WebSocketManager::Close(int id, websocketpp::close::status::value code, std::string reason)
{
  websocketpp::lib::error_code ec;

  con_list::iterator metadata_it = m_connection_list.find(id);
  if (metadata_it == m_connection_list.end()) {
    std::cout << "> No connection found with id " << id << std::endl;
    return;
  }

  m_endpoint.close(metadata_it->second->get_hdl(), code, reason, ec);
  if (ec) {
    std::cout << "> Error initiating close: " << ec.message() << std::endl;
  }
}

void WebSocketManager::Send(int id, std::string message)
{
  websocketpp::lib::error_code ec;

  con_list::iterator metadata_it = m_connection_list.find(id);
  if (metadata_it == m_connection_list.end()) {
    std::cout << "> No connection found with id " << id << std::endl;
    return;
  }

  m_endpoint.send(metadata_it->second->get_hdl(), message, websocketpp::frame::opcode::text, ec);
  if (ec) {
    std::cout << "> Error sending message: " << ec.message() << std::endl;
    return;
  }

  metadata_it->second->record_sent_message(message);
}

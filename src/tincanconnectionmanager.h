/*
 * ipop-tincan
 * Copyright 2013, University of Florida
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TINCAN_CONNECTIONMANAGER_H_
#define TINCAN_CONNECTIONMANAGER_H_
#pragma once

#include <string>
#include <map>
#include <set>

#include "webrtc/base/sigslot.h"
#include "webrtc/p2p/base/p2ptransport.h"
#include "webrtc/p2p/client/basicportallocator.h"
#include "webrtc/p2p/base/transportdescription.h"
#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/p2p/base/p2ptransportchannel.h"
#include "webrtc/p2p/base/dtlstransportchannel.h"
#include "webrtc/p2p/base/dtlstransport.h"
#include "webrtc/base/base64.h"
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/json.h"
#include "webrtc/base/sslidentity.h"

#include "talk/ipop-project/ipop-tap/src/ipop_tap.h"
#include "talk/ipop-project/ipop-tap/src/tap.h"
#include "talk/ipop-project/ipop-tap/src/peerlist.h"
#include "talk/ipop-project/ipop-tap/src/packetio.h"

#include "peersignalsender.h"
#include "wqueue.h"

namespace tincan {

static const char kTapName[] = "ipop";
static const char kTapDesc[] = "TAP";

class PeerSignalSender : public PeerSignalSenderInterface {
 public:
  // Inherited from PeerSignalSenderInterface
  virtual void SendToPeer(int overlay_id, const std::string& uid,
                          const std::string& data, const std::string& type) {
    return service_map_[overlay_id]->SendToPeer(overlay_id, uid, data, type);
  }

  virtual void add_service(int overlay_id, 
                           PeerSignalSenderInterface* sender) {
    service_map_[overlay_id] = sender;
  }

 private:
  std::map<int, PeerSignalSenderInterface*> service_map_;

};

class TinCanConnectionManager : public rtc::MessageHandler,
                                public sigslot::has_slots<> {

 public:
  TinCanConnectionManager(PeerSignalSenderInterface* signal_sender,
                          rtc::Thread* link_setup_thread,
                          rtc::Thread* packet_handling_thread,
                          thread_opts_t* opts);

  // Accessors
  const std::string fingerprint() const { return fingerprint_; }

  const std::string uid() const { return tincan_id_; }

  const std::string ipv4() const { return tincan_ip4_; }

  const std::string ipv6() const { return tincan_ip6_; }

  const std::string tap_name() const { return tap_name_; }
  
  rtc::Thread* packet_handling_thread() const { return packet_handling_thread_; }

  void set_ip(const char* ip) { tincan_ip4_ = ip; }

  void set_forward_addr(const rtc::SocketAddress addr) {
    forward_addr_ = addr;
  }

  void set_forward_socket(rtc::AsyncPacketSocket* socket) {
    forward_socket_ = socket;
  }

  void set_trim_connection(bool trim) {
    trim_enabled_ = trim;
  }

  void set_network_ignore_list(
      const std::vector<std::string>& network_ignore_list) {
    network_manager_.set_network_ignore_list(network_ignore_list);
  }

  // Signal handlers for BasicNetworkManager
  virtual void OnNetworksChanged();

  // Signal handlers for TransportChannelImpl
  virtual void OnRequestSignaling(cricket::Transport* transport);
  //virtual void OnRWChangeState(cricket::Transport* transport);
  virtual void OnReceivingState(cricket::TransportChannel* channel);
  virtual void OnWritableState(cricket::TransportChannel* channel);
  //kyuho
  //virtual void OnCandidatesReady(cricket::Transport* transport,
   //                              const cricket::Candidates& candidates);
  virtual void OnCandidatesReady(cricket::PortAllocatorSession* session,
                                 const cricket::Candidates& candidates);
  // kyuho
  //virtual void OnCandidatesAllocationDone(cricket::Transport* transport);
  virtual void OnCandidatesAllocationDone(cricket::PortAllocatorSession* session);
  virtual void OnReadPacket(cricket::TransportChannel* channel, 
                            const char* data, size_t len,
                            const rtc::PacketTime& ptime, int flags);

  // Inherited from MessageHandler
  virtual void OnMessage(rtc::Message* msg);

  // Signal handler for PeerSignalSenderInterface
  virtual void HandlePeer(const std::string& uid, const std::string& data,
                          const std::string& type);

  // Signal handler for PacketSenderInterface
  virtual void HandlePacket(rtc::AsyncPacketSocket* socket,
      const char* data, size_t len, const rtc::SocketAddress& addr);

  // Other public functions
  virtual void Setup(
      const std::string& uid, const std::string& ip4, int ip4_mask,
      const std::string& ip6, int ip6_mask, int subnet_mask, int switchmode);

  virtual bool CreateTransport(
      const std::string& uid, const std::string& fingerprint, int overlay_id,
      const std::string& stun_server, const std::string& turn_server,
      const std::string& turn_user, const std::string& turn_pass,
      bool sec_enabled);

  bool CreateConnections(const std::string& uid, 
                         const std::string& candidates_string);

  virtual bool AddIPMapping(const std::string& uid_key,
                            const std::string& ip4,
                            const std::string& ip6);

  virtual bool DestroyTransport(const std::string& uid);

  virtual Json::Value GetState(const std::map<std::string, uint32>& friends,
                               bool get_stats);

  static int DoPacketSend(const char* buf, size_t len);

  static int DoPacketRecv(char* buf, size_t len);

  static int SendToTap(const char* buf, size_t len);

  typedef cricket::DtlsTransport<cricket::P2PTransport> DtlsP2PTransport;

  struct PeerState {
    int overlay_id;
    uint32 last_time;
    std::string uid;
    std::string fingerprint;
    std::string connection_security;
    rtc::scoped_ptr<cricket::P2PTransport> transport;
    rtc::scoped_ptr<cricket::BasicPortAllocator> port_allocator;
    rtc::scoped_ptr<rtc::SSLFingerprint> remote_fingerprint;
    rtc::scoped_ptr<cricket::TransportDescription> local_description;
    rtc::scoped_ptr<cricket::TransportDescription> remote_description;
    // TODO do we really need this? -- kyuhoo 
    // I lost where this is assigned to. ...
    rtc::scoped_ptr<cricket::PortAllocatorSession> session; // --kyuho 
    cricket::P2PTransportChannel* channel;
    cricket::Candidates candidates;
    std::set<std::string> candidate_list;
    ~PeerState() {
      transport.reset();
      port_allocator.reset();
    }
  };

  struct PeerIPs {
    std::string ip4;
    std::string ip6;
  };

  typedef rtc::scoped_refptr<
      rtc::RefCountedObject<PeerState> > PeerStatePtr;

 private:
  void HandleConnectionSignal(cricket::Port* port,
                              cricket::Connection* connection);
  void SetupTransport(PeerState* peer_state);
  void HandleQueueSignal_w();
  void HandleControllerSignal_w();
  void InsertTransportMap_w(const std::string sub_uid,
                            cricket::Transport* transport);
  void DeleteTransportMap_w(const std::string sub_uid);
  Json::Value StateToJson(const std::string& uid, uint32 xmpp_time,
                          bool get_stats);
  bool SetRelay(PeerState* peer_state, const std::string& turn_server,
                const std::string& username, const std::string& password);
  void GetChannelStats_w(const std::string &uid,
                         cricket::ConnectionInfos *infos);
  bool is_icc(const unsigned char * buf);

  const std::string content_name_;
  PeerSignalSenderInterface* signal_sender_;
  rtc::BasicPacketSocketFactory packet_factory_;
  std::map<std::string, PeerStatePtr> uid_map_;
  std::map<std::string, cricket::Transport*> short_uid_map_;
  std::map<cricket::Transport*, std::string> transport_map_;
  std::map<cricket::PortAllocatorSession*, std::string> session_map_;
  std::map<std::string, PeerIPs> ip_map_;
  rtc::Thread* link_setup_thread_;
  rtc::Thread* packet_handling_thread_;
  rtc::BasicNetworkManager network_manager_;
  std::string tincan_id_;
  rtc::scoped_ptr<rtc::SSLIdentity> identity_;
  rtc::scoped_ptr<rtc::SSLFingerprint> local_fingerprint_;
  std::string fingerprint_;
  const uint64 tiebreaker_;
  std::string tincan_ip4_;
  std::string tincan_ip6_;
  std::string tap_name_;
  rtc::AsyncPacketSocket* forward_socket_;
  rtc::SocketAddress forward_addr_;
  rtc::PacketOptions packet_options_;
  bool trim_enabled_;
  thread_opts_t* opts_;
};

}  // namespace tincan

#endif  // TINCAN_CONNECTIONMANAGER_H_


//--------------------------------------------------------------------------
// Copyright (C) 2015-2018 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------

// tcp_state_closing.cc author davis mcpherson <davmcphe@cisco.com>
// Created on: Aug 5, 2015

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tcp_state_closing.h"

#include "tcp_normalizers.h"
#include "tcp_session.h"

#ifdef UNIT_TEST
#include "catch/snort_catch.h"
#endif

using namespace std;

TcpStateClosing::TcpStateClosing(TcpStateMachine& tsm) :
    TcpStateHandler(TcpStreamTracker::TCP_CLOSING, tsm)
{
}

bool TcpStateClosing::syn_sent(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    trk.session->check_for_repeated_syn(tsd);
    return true;
}

bool TcpStateClosing::syn_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    trk.normalizer.ecn_tracker(tsd.get_tcph(), trk.session->config->require_3whs());
    if ( tsd.get_seg_len() )
        trk.session->handle_data_on_syn(tsd);
    return true;
}

bool TcpStateClosing::ack_sent(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    trk.update_tracker_ack_sent(tsd);
    return true;
}

bool TcpStateClosing::ack_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    trk.update_tracker_ack_recv(tsd);
    if ( SEQ_GEQ(tsd.get_end_seq(), trk.rcv_nxt) )
        trk.set_tcp_state(TcpStreamTracker::TCP_TIME_WAIT);
    return true;
}

bool TcpStateClosing::data_seg_sent(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    trk.update_tracker_ack_sent(tsd);
    return true;
}

bool TcpStateClosing::data_seg_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    trk.update_tracker_ack_recv(tsd);
    if ( SEQ_GEQ(tsd.get_end_seq(), trk.rcv_nxt) )
        trk.set_tcp_state(TcpStreamTracker::TCP_TIME_WAIT);
    return true;
}

bool TcpStateClosing::fin_sent(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    trk.update_tracker_ack_sent(tsd);
    return true;
}

bool TcpStateClosing::fin_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    snort::Flow* flow = tsd.get_flow();

    trk.update_tracker_ack_recv(tsd);
    if ( SEQ_GT(tsd.get_seg_seq(), trk.get_fin_final_seq() ) )
    {
        trk.session->tel.set_tcp_event(EVENT_BAD_FIN);
        trk.normalizer.packet_dropper(tsd, NORM_TCP_BLOCK);
        trk.session->set_pkt_action_flag(ACTION_BAD_PKT);
    }

    if ( !flow->two_way_traffic() )
        trk.set_tf_flags(TF_FORCE_FLUSH);

    if ( SEQ_EQ(tsd.get_seg_ack(), trk.get_snd_nxt() ) )
        trk.set_tcp_state(TcpStreamTracker::TCP_TIME_WAIT);
    return true;
}

bool TcpStateClosing::rst_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    if ( trk.update_on_rst_recv(tsd) )
    {
        trk.session->update_session_on_rst(tsd, true);
        trk.session->update_perf_base_state(TcpStreamTracker::TCP_CLOSING);
        trk.session->set_pkt_action_flag(ACTION_RST);
        tsd.get_pkt()->flow->session_state |= STREAM_STATE_CLOSED;
    }
    else
    {
        trk.session->tel.set_tcp_event(EVENT_BAD_RST);
    }
    return true;
}

bool TcpStateClosing::do_pre_sm_packet_actions(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    return trk.session->validate_packet_established_session(tsd);
}

bool TcpStateClosing::do_post_sm_packet_actions(TcpSegmentDescriptor& tsd, TcpStreamTracker& trk)
{
    trk.session->update_paws_timestamps(tsd);
    trk.session->check_for_window_slam(tsd);
    return true;
}


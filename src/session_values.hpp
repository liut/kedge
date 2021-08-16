#pragma once


#include <libtorrent/session_stats.hpp>


struct sessionValues {

    // TODO: unchecked
    int const m_num_checking_idx = lt::find_metric_idx("ses.num_checking_torrents");
    int const m_num_stopped_idx = lt::find_metric_idx("ses.num_stopped_torrents");
    int const m_num_upload_only_idx = lt::find_metric_idx("ses.num_upload_only_torrents");
    int const m_num_downloading_idx = lt::find_metric_idx("ses.num_downloading_torrents");
    int const m_num_seeding_idx = lt::find_metric_idx("ses.num_seeding_torrents");
    int const m_num_queued_seeding_idx = lt::find_metric_idx("ses.num_queued_seeding_torrents");
    int const m_num_queued_download_idx = lt::find_metric_idx("ses.num_queued_download_torrents");
    int const m_num_error_idx = lt::find_metric_idx("ses.num_error_torrents");

    int const m_queued_bytes_idx = lt::find_metric_idx("disk.queued_write_bytes");
    int const m_wasted_bytes_idx = lt::find_metric_idx("net.recv_redundant_bytes");
    int const m_failed_bytes_idx = lt::find_metric_idx("net.recv_failed_bytes");
    int const m_num_peers_connected_idx = lt::find_metric_idx("peer.num_peers_connected");
    int const m_num_peers_half_open_idx = lt::find_metric_idx("peer.num_peers_half_open");
    int const m_has_incoming_idx = lt::find_metric_idx("net.has_incoming_connections");
    int const m_recv_idx = lt::find_metric_idx("net.recv_bytes");
    int const m_sent_idx = lt::find_metric_idx("net.sent_bytes");
    int const m_recv_data_idx = lt::find_metric_idx("net.recv_payload_bytes");
    int const m_send_data_idx = lt::find_metric_idx("net.sent_payload_bytes");
    int const m_unchoked_idx = lt::find_metric_idx("peer.num_peers_up_unchoked");
    int const m_unchoke_slots_idx = lt::find_metric_idx("ses.num_unchoke_slots");
    int const m_limiter_up_queue_idx = lt::find_metric_idx("net.limiter_up_queue");
    int const m_limiter_down_queue_idx = lt::find_metric_idx("net.limiter_down_queue");
    int const m_queued_writes_idx = lt::find_metric_idx("disk.num_write_jobs");
    int const m_queued_reads_idx = lt::find_metric_idx("disk.num_read_jobs");

    int const m_writes_cache_idx = lt::find_metric_idx("disk.write_cache_blocks");
    int const m_reads_cache_idx = lt::find_metric_idx("disk.read_cache_blocks");
    int const m_pinned_idx = lt::find_metric_idx("disk.pinned_blocks");
    int const m_num_blocks_read_idx = lt::find_metric_idx("disk.num_blocks_read");
    int const m_cache_hit_idx = lt::find_metric_idx("disk.num_blocks_cache_hits");
    int const m_blocks_in_use_idx = lt::find_metric_idx("disk.disk_blocks_in_use");
    int const m_blocks_written_idx = lt::find_metric_idx("disk.num_blocks_written");
    int const m_write_ops_idx = lt::find_metric_idx("disk.num_write_ops");

    int const m_mfu_size_idx = lt::find_metric_idx("disk.arc_mfu_size");
    int const m_mfu_ghost_idx = lt::find_metric_idx("disk.arc_mfu_ghost_size");
    int const m_mru_size_idx = lt::find_metric_idx("disk.arc_mru_size");
    int const m_mru_ghost_idx = lt::find_metric_idx("disk.arc_mru_ghost_size");

    int const m_utp_idle = lt::find_metric_idx("utp.num_utp_idle");
    int const m_utp_syn_sent = lt::find_metric_idx("utp.num_utp_syn_sent");
    int const m_utp_connected = lt::find_metric_idx("utp.num_utp_connected");
    int const m_utp_fin_sent = lt::find_metric_idx("utp.num_utp_fin_sent");
    int const m_utp_close_wait = lt::find_metric_idx("utp.num_utp_close_wait");

    int const m_queued_tracker_announces = lt::find_metric_idx("tracker.num_queued_tracker_announces");

};


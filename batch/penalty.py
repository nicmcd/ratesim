def computePenalty(stats):
  # this takes a weighted average of the latencies
  #  currently all three sections have the same weight
  lat_ov = sum(((stats[1]['4-9s'] - 500) * 1.0,
                (stats[2]['4-9s'] - 500) * 1.0,
                (stats[3]['4-9s'] - 500) * 1.0)) / 3

  # this takes the a weighted maximum of the bandwidths
  #  currently the 3rd section has a 2x weight because during this time the
  #  senders are sending at the same rate
  bw_ov = max((stats[1]['bw'] * 1.0,
               stats[2]['bw'] * 1.0,
               stats[3]['bw'] * 2.0))

  # 'coeff' nanoseconds is worth 1 phits/sec of bandwidth
  coeff = 100.0
  return lat_ov + coeff * bw_ov

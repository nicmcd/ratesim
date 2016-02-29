def computePenalty(stats):
  bw_ov = stats[3]['bw']
  lat_ov = stats[2]['4-9s'] - 500


  bw_pen = bw_ov
  lat_pen = lat_ov / 100

  return bw_pen + lat_pen

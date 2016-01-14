#!/usr/bin/env python3

import argparse
import json
import rateparse
import matplotlib.pyplot as plt
import numpy

def main(args):
  # get the RawData object
  raw = rateparse.RawData(args.input)

  # if requested, save the raw data to a file
  if args.output:
    raw.write(args.output)

  # create a quad-plot
  fig = plt.figure(figsize=(16,10))
  ax1 = fig.add_subplot(2, 2, 1)
  ax2 = fig.add_subplot(2, 2, 2)
  ax3 = fig.add_subplot(2, 2, 3)
  ax4 = fig.add_subplot(2, 2, 4)

  # get the xlim to be used by all time-based plots
  xlim = getXlim(raw)

  # plot data
  bulkAggregates(ax1, raw, xlim, smoothness=args.smoothness)
  bandwidthOverhead(ax2, raw, xlim, smoothness=args.smoothness)
  wireLatencyScatter(ax3, raw, xlim)
  totalLatencyScatter(ax4, raw, xlim)

  fig.tight_layout()

  if args.viewplot:
    plt.show()

  if args.plotfile:
    fig.savefig(args.plotfile)


def getXlim(raw):
  # extract latency data
  times, _ = raw.extractLatencies('onwire')
  return max(times) * 1.02


def bulkAggregates(ax, raw, xlim, smoothness):
  # extract the data
  ss = raw.extractRate(['Sender_*'], 'send')
  sr = raw.extractRate(['Sender_*'], 'recv')
  rr = raw.extractRate(['Receiver_*'], 'recv')

  # apply smoothing
  ssb = smooth(ss, smoothness)
  srb = smooth(sr, smoothness)
  rrb = smooth(rr, smoothness)

  # plot the data
  time = numpy.arange(raw.stats['Total simulation ticks'] + 1)
  ssl = ax.plot(time, ssb, 'r', label='Senders send')
  srl = ax.plot(time, srb, 'g', label='Senders recv')
  rrl = ax.plot(time, rrb, 'b', label='Receivers recv')

  # plot the rate limit
  limit = [raw.settings['rate_limit']] * len(time)
  ll = ax.plot(time, limit, 'y--', linewidth=4)

  # format axis
  ax.set_xlabel('Time (cycles)')
  ax.set_ylabel('Bandwidth (flits/cycle)')
  ax.set_xlim(0, xlim)
  ax.set_ylim([0, max(raw.settings['rate_limit'] * 1.1, max(rrb) * 1.50)])
  ax.legend()
  ax.grid(True)
  ax.set_title('Bandwidths by Group')


def bandwidthOverhead(ax, raw, xlim, smoothness):
  # extract the data
  total_rate = raw.extractRate(['.*'], 'recv')
  receiver_rate = raw.extractRate(['Receiver_.*'], 'recv')
  rate = numpy.subtract(total_rate, receiver_rate)

  # apply smoothing
  rate = smooth(rate, smoothness)

  # plot the data
  time = numpy.arange(raw.stats['Total simulation ticks'] + 1)
  line = ax.plot(time, rate, 'm')

  # format axis
  ax.set_xlabel('Time (cycles)')
  ax.set_ylabel('Bandwidth (flits/cycle)')
  ax.set_title('Bandwidth Overhead')
  ax.set_xlim(0, xlim)
  ax.grid(True)


def wireLatencyScatter(ax, raw, xlim):
  # extract latency data
  times, latencies = raw.extractLatencies('onwire')

  # scatter plot the data
  sc = ax.scatter(times, latencies, color='b', s=2)

  # format axis
  ax.set_xlabel('Time (cycles)')
  ax.set_ylabel('Latency (cycles)')
  ax.set_title('NIC-to-NIC Latency')
  ax.set_xlim(0, xlim)
  ax.set_ylim(0, max(latencies) * 1.05)
  ax.grid(True)


def totalLatencyScatter(ax, raw, xlim):
  # extract latency data
  times, latencies = raw.extractLatencies('total')

  # scatter plot the data
  sc = ax.scatter(times, latencies, color='b', s=2)

  # format axis
  ax.set_xlabel('Time (cycles)')
  ax.set_ylabel('Latency (cycles)')
  ax.set_title('End-to-End Latency')
  ax.set_xlim(0, xlim)
  ax.set_ylim(0, max(latencies) * 1.05)
  ax.grid(True)


def smooth(data, radius):
  ndata = data.copy()
  size = len(ndata)

  # quick bail out when no work needed
  if radius < 1:
    return ndata

  # process each data point
  for idx in range(0, size):
    bot = max(0, idx - radius)
    top = min(size - 1, idx + radius)
    cnt = top + 1 - bot
    sum = 0
    #print('{0} {1} {2} {3}'.format(idx, bot, top, cnt))
    for idx2 in range(bot, top+1):
      sum += data[idx2]
    ndata[idx] = sum / cnt

  return ndata

if __name__ == '__main__':
  """
  data = [1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
          1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
          1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
          1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0]
  ndata = smooth(data, 4)
  print('{0}\n{1}'.format(data, ndata))
  import sys
  sys.exit(0)
  """

  ap = argparse.ArgumentParser()
  ap.add_argument('input',
                  help='input data or log file')
  ap.add_argument('-o', '--output',
                  help='save raw data to file')
  ap.add_argument('-s', '--smoothness', type=int, default=0,
                  help='smoothing factor applied to all data')
  ap.add_argument('-p', '--plotfile', default=None,
                  help='filename of output plot file')
  ap.add_argument('-v', '--viewplot', action='store_true',
                  help='show plot GUI')
  main(ap.parse_args())

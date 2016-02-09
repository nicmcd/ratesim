#!/usr/bin/env python3

import argparse
import json
import os
if 'DISPLAY' not in os.environ or os.environ['DISPLAY'] == '':
  import matplotlib
  matplotlib.use('Agg')
import matplotlib.pyplot as plt

import rateparse
import numpy


def main(args):
  if args.output or args.plotfile or args.viewplot:
    # get the RawData object
    print('parsing')
    raw = rateparse.RawData(args.input)

  # if requested, save the raw data to a file
  if args.output:
    print('writing raw file')
    raw.write(args.output)

  if args.plotfile or args.viewplot:
    # create a quad-plot
    fig = plt.figure(figsize=(16, 9))
    ax1 = fig.add_subplot(2, 3, 1)
    ax2 = fig.add_subplot(2, 3, 2)
    ax3 = fig.add_subplot(2, 3, 3)
    ax4 = fig.add_subplot(2, 3, 4)
    ax5 = fig.add_subplot(2, 3, 5)
    ax6 = fig.add_subplot(2, 3, 6)

    # get the xlim to be used by all time-based plots
    xlim = getXlim(raw)

    # plot data
    print('plotting bulk aggregates')
    bulkAggregatesPlot(ax1, raw, xlim, smoothness=args.smoothness)
    print('plotting bandwidth overhead')
    bandwidthOverheadPlot(ax2, raw, xlim, smoothness=args.smoothness)
    print('plotting wire latency')
    wireLatencyScatterPlot(ax3, raw, xlim)
    print('plotting total latency')
    totalLatencyScatterPlot(ax4, raw, xlim)
    print('plotting latency percentiles in section 1')
    latencyPercentilesPlot(ax5, raw, 2000, 10000)
    print('plotting latency percentiles in section 2')
    latencyPercentilesPlot(ax6, raw, 18000, 25000)

    fig.tight_layout()

    if args.viewplot:
      print('showing plot')
      plt.show()

    if args.plotfile:
      print('saving plot file')
      fig.savefig(args.plotfile)

  if args.datafile:
    print('writing data file')
    with open(args.datafile, 'w') as df:
      for sect, (xmin, xmax) in enumerate([(2000, 10000), (18000, 25000)]):
        print('Section #{0}'.format(sect), file=df)
        bw = bandwidthOverhead(raw, xmin, xmax)
        print('bandwidth overhead = {0}'.format(bw), file=df)
        p29 = latencyPercentile(raw, xmin, xmax, 0.99)
        print('99%ile latency = {0}'.format(p29), file=df)
        p39 = latencyPercentile(raw, xmin, xmax, 0.999)
        print('99.9%ile latency = {0}'.format(p39), file=df)
        p49 = latencyPercentile(raw, xmin, xmax, 0.9999)
        print('99.99%ile latency = {0}'.format(p49), file=df)
        print('', file=df)

  print('done')

def getXlim(raw):
  # extract latency data
  times, _ = raw.extractLatencies('onwire')
  return max(times) * 1.02


def bulkAggregatesPlot(ax, raw, xlim, smoothness):
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
  ax.set_ylabel('Bandwidth (phits/cycle)')
  ax.set_xlim(0, xlim)
  ax.set_ylim([0, max(raw.settings['rate_limit'] * 1.1, max(rrb) * 1.50)])
  ax.legend()
  ax.grid(True)
  ax.set_title('Bandwidths by Group')


def bandwidthOverheadPlot(ax, raw, xlim, smoothness):
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
  ax.set_ylabel('Bandwidth (phits/cycle)')
  ax.set_title('Bandwidth Overhead')
  ax.set_xlim(0, xlim)
  ax.grid(True)


def bandwidthOverhead(raw, xmin, xmax):
  # extract the data
  total_rate = raw.extractRate(['.*'], 'recv')
  receiver_rate = raw.extractRate(['Receiver_.*'], 'recv')
  rate = numpy.subtract(total_rate, receiver_rate)

  # average the range
  return numpy.average(rate[xmin:xmax+1])


def wireLatencyScatterPlot(ax, raw, xlim):
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


def totalLatencyScatterPlot(ax, raw, xlim):
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


def latencyPercentilesPlot(ax, raw, xmin, xmax):
  # extract latency data
  _, latencies = raw.extractLatencies('total', bounds=[xmin, xmax])

  # compute the distribution
  cdfx = numpy.sort(latencies)
  cdfy = numpy.linspace(1 / len(latencies), 1.0, len(latencies))

  # scatter plot
  ax.scatter(cdfx, cdfy, color='b', s=2)

  # format axis
  ax.set_xlabel('Latency (cycles)')
  if cdfx[0] == cdfx[-1]:
    ax.set_xlim([cdfx[0] - 3, cdfx[0] + 3])
  ax.set_ylabel('Percentile')
  ax.set_title('Log CDF ({0}-{1})'.format(xmin, xmax))
  ax.set_yscale('close_to_one', nines=5)
  ax.grid(True)


def latencyPercentile(raw, xmin, xmax, percentile):
  # extract latency data
  _, latencies = raw.extractLatencies('total', bounds=[xmin, xmax])

  # compute the distribution
  cdfx = numpy.sort(latencies)
  cdfy = numpy.linspace(1 / len(latencies), 1.0, len(latencies))

  # retrieve the percentile sample latency
  assert percentile > 0.0 and percentile <= 1.0
  index = len(cdfx) * percentile
  return cdfx[index]


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
  ap = argparse.ArgumentParser()
  ap.add_argument('input',
                  help='input data or log file')
  ap.add_argument('-o', '--output',
                  help='save raw data to file')
  ap.add_argument('-s', '--smoothness', type=int, default=0,
                  help='smoothing factor applied to all data')
  ap.add_argument('-p', '--plotfile', default=None,
                  help='filename of output plot file')
  ap.add_argument('-d', '--datafile', default=None,
                  help='output data file')
  ap.add_argument('-v', '--viewplot', action='store_true',
                  help='show plot GUI')
  main(ap.parse_args())

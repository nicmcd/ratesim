#!/usr/bin/env python3

import argparse
import json
import os
if 'DISPLAY' not in os.environ or os.environ['DISPLAY'] == '':
  import matplotlib
  matplotlib.use('Agg')
import matplotlib.pyplot as plt

from CloseToOne import *
from RawData import *
import numpy


def main(args):
  #############################################################################
  # Parsing
  #############################################################################
  print('parsing')

  # get the RawData object
  raw = RawData(args.input)

  # if requested, save the raw data to a file
  if args.output:
    print('writing raw file')
    raw.write(args.output)

  #############################################################################
  # Analyzing
  #############################################################################
  print('analyzing')

  # extract needed information
  time = numpy.arange(raw.stats['Total simulation ticks'] + 1)
  limit = numpy.multiply(numpy.ones(len(time)), raw.settings['rate_limit'])
  total_recv = raw.extractRate(['.*'], 'recv')
  sender_send = raw.extractRate(['Sender_.*'], 'send')
  sender_recv = raw.extractRate(['Sender_.*'], 'recv')
  receiver_recv = raw.extractRate(['Receiver_.*'], 'recv')
  times, latencies = raw.extractLatencies('total')

  _, sect1_latencies = raw.extractLatencies('total', bounds=[10000, 50000])
  _, sect2_latencies = raw.extractLatencies('total', bounds=[50000, 90000])
  _, sect3_latencies = raw.extractLatencies('total', bounds=[90000, 130000])

  # compute stats from extracted data
  overhead_recv = numpy.subtract(total_recv, receiver_recv)
  sect1_cdf = getCdf(sect1_latencies)
  sect2_cdf = getCdf(sect2_latencies)
  sect3_cdf = getCdf(sect3_latencies)

  # apply smoothing as needed
  smooth_sender_send = smooth(sender_send, args.smooth)
  smooth_sender_recv = smooth(sender_recv, args.smooth)
  smooth_receiver_recv = smooth(receiver_recv, args.smooth)
  smooth_overhead_recv = smooth(overhead_recv, args.smooth)

  #############################################################################
  # Writing stats
  #############################################################################
  print('writing stats')

  with open(args.datafile, 'w') as df:
    for sect, bounds, cdf in [(1, (10000, 50000), sect1_cdf),
                              (2, (50000, 90000), sect2_cdf),
                              (3, (90000, 130000), sect3_cdf)]:
      print('Section #{0}'.format(sect), file=df)
      xmin, xmax = bounds
      cdfx, cdfy = cdf
      bw = numpy.average(overhead_recv[xmin:xmax+1])
      print('bandwidth overhead = {0}'.format(bw), file=df)
      p29 = getPercentile(cdfx, 0.99)
      print('99%ile latency = {0}'.format(p29), file=df)
      p39 = getPercentile(cdfx, 0.999)
      print('99.9%ile latency = {0}'.format(p39), file=df)
      p49 = getPercentile(cdfx, 0.9999)
      print('99.99%ile latency = {0}'.format(p49), file=df)
      p59 = getPercentile(cdfx, 0.9999)
      print('99.999%ile latency = {0}'.format(p59), file=df)
      print('', file=df)

  #############################################################################
  # Plotting
  #############################################################################
  print('plotting')

  # create a quad-plot
  fig = plt.figure(figsize=(16, 9))
  ax1 = fig.add_subplot(2, 3, 1)
  ax2 = fig.add_subplot(2, 3, 2)
  ax3 = fig.add_subplot(2, 3, 3)
  ax4 = fig.add_subplot(2, 3, 4)
  ax5 = fig.add_subplot(2, 3, 5)
  ax6 = fig.add_subplot(2, 3, 6)

  # get the xlim to be used by all time-based plots
  xlim = max(times) * 1.02

  # plot the bulk aggregates
  ax = ax1
  ax.plot(time, smooth_sender_send, 'r', label='Senders send')
  ax.plot(time, smooth_sender_recv, 'g', label='Senders recv')
  ax.plot(time, smooth_receiver_recv, 'b', label='Receivers recv')
  ax.plot(time, limit, 'y--', linewidth=4, label='Rate limit')
  ax.set_xlabel('Time (cycles)')
  ax.set_ylabel('Bandwidth (phits/cycle)')
  ax.set_title('Bandwidths by Group')
  ax.set_xlim(0, xlim)
  ax.set_ylim([0, raw.settings['rate_limit'] * 1.2])
  ax.legend(fontsize=10)
  ax.grid(True)

  # plot the bandwidth overhead
  ax = ax2
  ax.plot(time, smooth_overhead_recv, 'm')
  ax.set_xlabel('Time (cycles)')
  ax.set_ylabel('Bandwidth (phits/cycle)')
  ax.set_title('Bandwidth Overhead')
  ax.set_xlim(0, xlim)
  ax.grid(True)

  # plot the end-to-end latencies
  ax = ax3
  ax.scatter(times, latencies, color='b', s=2)
  ax.set_xlabel('Time (cycles)')
  ax.set_ylabel('Latency (cycles)')
  ax.set_title('End-to-End Latency')
  ax.set_xlim(0, xlim)
  ax.set_ylim(0, max(latencies) * 1.05)
  ax.grid(True)

  # plot the latency percentiles in section 1, 2, and 3
  for ax, bounds, cdf in [(ax4, (10000, 50000), sect1_cdf),
                          (ax5, (50000, 90000), sect2_cdf),
                          (ax6, (90000, 130000), sect3_cdf)]:
    xmin, xmax = bounds
    cdfx, cdfy = cdf
    ax.scatter(cdfx, cdfy, color='b', s=2)
    ax.set_xlabel('Latency (cycles)')
    if cdfx[0] == cdfx[-1]:
      ax.set_xlim([cdfx[0] - 3, cdfx[0] + 3])
    ax.set_ylabel('Percentile')
    ax.set_title('Log CDF ({0}-{1})'.format(xmin, xmax))
    ax.set_yscale('close_to_one', nines=5)
    ax.grid(True)

  # tidy up and plot
  fig.tight_layout()
  fig.savefig(args.plotfile)


def getCdf(latencies):
  # compute the distribution
  cdfx = numpy.sort(latencies)
  cdfy = numpy.linspace(1 / len(latencies), 1.0, len(latencies))
  return (cdfx, cdfy)


def getPercentile(cdfx, percentile):
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


def scaleXaxis(ax, tickRange, divisor, unit):
  # format ticks and labels
  ticks = []
  labels = []
  for val in tickRange:
    ticks.append(val)
    labels.append('{0}{1}'.format(val // divisor, unit))

  # set values
  ax.set_xticks(ticks)
  ax.set_xticklabels(labels)



if __name__ == '__main__':
  ap = argparse.ArgumentParser()
  ap.add_argument('input',
                  help='input data or log file')
  ap.add_argument('plotfile',
                  help='filename of output plot file')
  ap.add_argument('datafile',
                  help='output data file')
  ap.add_argument('-o', '--output',
                  help='save raw data to file')
  ap.add_argument('-s', '--smooth', type=int, default=0,
                  help='smoothing factor applied to all data')
  main(ap.parse_args())

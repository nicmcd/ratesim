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
  if args.raw:
    print('writing raw file')
    raw.write(args.raw)

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
  send_times, latencies = raw.extractLatencies('total')

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
      p59 = getPercentile(cdfx, 0.99999)
      print('99.999%ile latency = {0}'.format(p59), file=df)
      print('', file=df)

  #############################################################################
  # Plotting
  #############################################################################
  print('plotting')

  # create a figures and axes
  mp_fig = plt.figure(figsize=(16, 9))
  mp_ax_bwa = mp_fig.add_subplot(2, 3, 1)
  mp_ax_bwo = mp_fig.add_subplot(2, 3, 2)
  mp_ax_lat = mp_fig.add_subplot(2, 3, 3)
  mp_ax_lp1 = mp_fig.add_subplot(2, 3, 4)
  mp_ax_lp2 = mp_fig.add_subplot(2, 3, 5)
  mp_ax_lp3 = mp_fig.add_subplot(2, 3, 6)

  bwa_fig = plt.figure(figsize=(5, 4))
  bwa_ax = bwa_fig.add_subplot(1, 1, 1)

  bwo_fig = plt.figure(figsize=(5, 4))
  bwo_ax = bwo_fig.add_subplot(1, 1, 1)

  lat_fig = plt.figure(figsize=(5, 4))
  lat_ax = lat_fig.add_subplot(1, 1, 1)

  lp1_fig = plt.figure(figsize=(5, 4))
  lp1_ax = lp1_fig.add_subplot(1, 1, 1)

  lp2_fig = plt.figure(figsize=(5, 4))
  lp2_ax = lp2_fig.add_subplot(1, 1, 1)

  lp3_fig = plt.figure(figsize=(5, 4))
  lp3_ax = lp3_fig.add_subplot(1, 1, 1)

  # get the xlim to be used by all time-based plots
  xlim = max(send_times) * 1.02

  # plot the bandwidth aggregates
  def plotBandwidthAggregates(ax, title, legend):
    ax.plot(time, smooth_sender_send, 'r', label='Senders send')
    ax.plot(time, smooth_sender_recv, 'g', label='Senders recv')
    ax.plot(time, smooth_receiver_recv, 'b', label='Receivers recv')
    ax.plot(time, limit, 'y--', linewidth=4, label='Rate limit')
    ax.set_xlabel('Time (cycles)')
    ax.set_ylabel('Bandwidth (phits/cycle)')
    if title:
      ax.set_title('Bandwidths by Group')
    ax.set_xlim(0, xlim)
    ax.set_ylim([0, raw.settings['rate_limit'] * 1.2])
    ax.locator_params(axis='x', nbins=6)
    if legend:
      ax.legend(fontsize=10)
    ax.grid(True)

  plotBandwidthAggregates(mp_ax_bwa, True, True)
  plotBandwidthAggregates(bwa_ax, False, False)

  # plot the bandwidth overhead
  def plotBandwidthOverhead(ax, title):
    ax.plot(time, smooth_overhead_recv, 'm')
    ax.set_xlabel('Time (cycles)')
    ax.set_ylabel('Bandwidth (phits/cycle)')
    if title:
      ax.set_title('Bandwidth Overhead')
    ax.set_xlim(0, xlim)
    ax.locator_params(axis='x', nbins=6)
    ax.grid(True)

  plotBandwidthOverhead(mp_ax_bwo, True)
  plotBandwidthOverhead(bwo_ax, False)

  # plot the end-to-end latencies
  def plotLatencies(ax, title):
    ax.scatter(send_times, latencies, color='b', s=2)
    ax.set_xlabel('Time (cycles)')
    ax.set_ylabel('Latency (cycles)')
    if title:
      ax.set_title('End-to-End Latency')
    ax.set_xlim(0, xlim)
    ax.set_ylim(0, max(latencies) * 1.05)
    ax.locator_params(axis='x', nbins=6)
    ax.grid(True)

  plotLatencies(mp_ax_lat, True)
  plotLatencies(lat_ax, False)

  # plot the latency percentiles in section 1, 2, and 3
  def plotPercentiles(ax, xmin, xmax, cdf, title):
    cdfx, cdfy = cdf
    ax.scatter(cdfx, cdfy, color='b', s=2)
    ax.set_xlabel('Latency (cycles)')
    if cdfx[0] == cdfx[-1]:
      ax.set_xlim([cdfx[0] - 3, cdfx[0] + 3])
    ax.set_ylabel('Percentile')
    if title:
      ax.set_title('Log CDF ({0}-{1})'.format(xmin, xmax))
    ax.set_yscale('close_to_one', nines=5)
    ax.locator_params(axis='x', nbins=6)
    ax.grid(True)

  for ax, bounds, cdf, title in [(mp_ax_lp1, (10000, 50000), sect1_cdf, True),
                                 (mp_ax_lp2, (50000, 90000), sect2_cdf, True),
                                 (mp_ax_lp3, (90000, 130000), sect3_cdf, True),
                                 (lp1_ax, (10000, 50000), sect1_cdf, False),
                                 (lp2_ax, (50000, 90000), sect2_cdf, False),
                                 (lp3_ax, (90000, 130000), sect3_cdf, False)]:
    xmin, xmax = bounds
    plotPercentiles(ax, xmin, xmax, cdf, title)

  # tidy up and plot
  for fig, path in [(mp_fig, args.multiplot),
                    (bwa_fig, args.bwa_plot),
                    (bwo_fig, args.bwo_plot),
                    (lat_fig, args.lat_plot),
                    (lp1_fig, args.lp1_plot),
                    (lp2_fig, args.lp2_plot),
                    (lp3_fig, args.lp3_plot)]:
    fig.tight_layout()
    fig.savefig(path)


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
  ap.add_argument('datafile',
                  help='output data file')
  ap.add_argument('multiplot',
                  help='output multiplot file')
  ap.add_argument('bwa_plot',
                  help='bandwidth aggregate plot file')
  ap.add_argument('bwo_plot',
                  help='bandwidth overhead plot file')
  ap.add_argument('lat_plot',
                  help='latency plot file')
  ap.add_argument('lp1_plot',
                  help='latency percentile section 1 plot file')
  ap.add_argument('lp2_plot',
                  help='latency percentile section 2 plot file')
  ap.add_argument('lp3_plot',
                  help='latency percentile section 3 plot file')
  ap.add_argument('-r', '--raw',
                  help='save raw data to file')
  ap.add_argument('-s', '--smooth', type=int, default=0,
                  help='smoothing factor applied to all data')

  main(ap.parse_args())

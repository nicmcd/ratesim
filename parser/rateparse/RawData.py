import collections
import gzip
import json
import numpy
import pickle
import re

class RawData(object):
  """
  This class holds raw data from a parse of a ratesim log file.
  """

  def __init__(self, filename):
    """
    This initializes a RawData object from the specified file.

    Args:
      filename (string) : can be any file holding raw ratesim data.
        examples:
          *.log    : this is an uncompressed log file directly from ratesim
          *.log.gz : this is a compressed log file directly from ratesim
          *.dat    : this is an uncompressed raw data file
          *.dat.gz : this is a compressed raw data file
    """

    # parse the file based on its type
    if filename.endswith('.gz'):
      with gzip.open(filename, 'rb') as fd:
        if filename.endswith('.dat.gz'):
          self.settings, self.data, self.transactions, self.stats = \
              pickle.load(fd)
        elif filename.endswith('.log.gz'):
          self.settings, self.data, self.transactions, self.stats = \
              self.parse(fd)
        else:
          raise Exception('invalid file type: {0}'.format(filename))
    else:
      with open(filename, 'rb') as fd:
        if filename.endswith('.dat'):
          self.settings, self.data, self.transactions, self.stats = \
              pickle.load(fd)
        elif filename.endswith('.log'):
          self.settings, self.data, self.transactions, self.stats = \
              self.parse(fd)
        else:
          raise Exception('invalid file type: {0}'.format(filename))

  def parse(self, fd):
    """
    This parses a log file into RawData

    Args:
      fd (file descriptor) : the open file to be parsed

    Return:
      (settings, actions, transactions, stats) :
        'settings' is loaded from the JSON representing the configuration
        'actions' is a nested dict with all send and receive actions
        'transactions' is a nested dict with all transactions
        'stats' is a dict with simulation stats
    """

    print('parsing')
    mode = 'settings'
    settings = ''
    actions = {}
    transactions = {}
    stats = {}

    for line in fd:
      line = line.decode('utf-8')

      # read in settings
      if mode == 'settings':
        settings += line
        if line == '}\n':
          mode = 'data'
          settings = json.loads(settings)

      # read bulk data
      elif mode == 'data':
        # find last line
        if line.strip() == '':
          mode = 'stats'

        elif line[0] == '[':
          time_split = line.find(':')
          time_end = line.find(']')
          tick = int(line[1:time_split])
          epsilon = int(line[time_split+1 : time_end])
          line = line[time_end+1:]

          name_end = line.find(':')
          name = line[:name_end].strip()
          line = line[name_end+1:]

          func_end = line.find(':')
          func = line[:func_end].strip()
          line = line[func_end+1:]

          elems_start = line.find('|') + 1
          line = line[elems_start:]
          elems = {}
          for elem in line.split():
            sep = elem.find('=')
            key = elem[:sep]
            val = int(elem[sep+1:])
            elems[key] = val

          cnode = elems['src'] if func == 'handle_send' else elems['dst']

          if name not in actions:
            actions[name] = {'id': cnode, 'send': [], 'recv': []}

          atype = 'recv' if func == 'handle_recv' else 'send'
          onode = elems['src'] if func == 'handle_recv' else elems['dst']
          actions[name][atype].append((tick, onode, elems['size'],
                                       elems['trans'], elems['type']))

          # do transaction handling
          if func == 'handle_send':
            if elems['trans'] not in transactions:
              transactions[elems['trans']] = {'msgs': 1, 'start': tick,
                                              'end': 0}
            else:
              transactions[elems['trans']]['msgs'] += 1
              transactions[elems['trans']]['start'] = \
                  min(transactions[elems['trans']]['start'], tick)
          else:
            assert func == 'handle_recv'
            transactions[elems['trans']]['end'] = \
                max(transactions[elems['trans']]['end'], tick)
        else:
          raise Exception('WTF line is this??: {0}'.format(line))

      # gather stats
      elif mode == 'stats':
        split = line.find(':')
        if split > 0:
          stat_name = line[0:split]
          try:
            stat_val = int(line[split+1:])
          except:
            stat_val = float(line[split+1:])
          #print('{0} -> {1}'.format(stat_name, stat_val))
          stats[stat_name] = stat_val

      # inv mode
      else:
        raise Exception('invalid mode: {0}'.format(mode))

    print('done parsing')
    return (settings, actions, transactions, stats)

  def write(self, filename):
    """
    This writes this RawData object to a file

    Args:
      filename (string) : the filename to store the data
                          (end with .gz for compressions)
    """
    everything = (self.settings, self.data, self.transactions, self.stats)

    if filename.endswith('.gz'):
      if not filename.endswith('.dat.gz'):
        raise Exception('invalid filename: {0}'.format(filename))
      with gzip.open(filename, 'wb') as fd:
        pickle.dump(everything, fd)
    else:
      if not filename.endswith('.dat'):
        raise Exception('invalid filename: {0}'.format(filename))
      with open(filename, 'wb') as fd:
        pickle.dump(everything, fd)

  def extractRate(self, regexs, send, rate=None):
    """
    This extracts a rate vector from a select group of nodes.

    Args:
      regexs (collection) : regexs to use to match with node names.
      send (bool) : true to select send ops, false to select recv ops
      rate (Counter) : the numpy array to use for storing rates

    Return:
      (numpy array) : the rate as a numpy array
    """

    if rate:
      assert len(rate) == self.stats['Total simulation ticks'] + 1
    else:
      rate = numpy.zeros(self.stats['Total simulation ticks'] + 1)

    for name in self.data.keys():
      for regex in regexs:
        m = re.match(regex, name)
        if not m:
          #print('failed {0} against {1}'.format(name, regex))
          pass
        else:
          #print('passed {0} against {1}'.format(name, regex))

          # get all actions of the specified type from this node
          dev = self.data[name]
          id = dev['id']
          actions = dev['send'] if send else dev['recv']

          # handle each action
          for tick, onode, size, trans, type in actions:
            #print('{0} {1} {2} {3} {4}'.format(tick, onode, size, trans, type))

            # send ops tick is before
            if send:
              for phit_time in range(tick, tick + size):
                rate[phit_time] += 1

            # receive ops tick is after
            else:
              for phit_time in range(tick - size + 1, tick + 1):
                rate[phit_time] += 1

          # if this one matched, then quit the search
          break

    return rate

  def extractLatencies(self):
    times = []
    latencies = []

    # find all latencies
    for trans in self.transactions:
      start = self.transactions[trans]['start']
      end = self.transactions[trans]['end']
      times.append(end)
      latencies.append(end - start + 1)

    # return times and latencies
    return times, latencies

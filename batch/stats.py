def parseStats(filename, verbose=False):
  strmap = {
    'bandwidth overhead': 'bw',
    '99%ile latency': '2-9s',
    '99.9%ile latency': '3-9s',
    '99.99%ile latency': '4-9s',
    '99.999%ile latency': '5-9s'
  }

  stats = {}
  sect = None
  with open(filename, 'r') as fd:
    for line in fd:
      line = line.strip()
      if line.startswith('Section #'):
        sect = int(line[len('Section #'):])
        assert sect not in stats
        stats[sect] = {}
        if verbose:
          print('section {0}'.format(sect))
      elif line.find('=') > -1:
        stat, value = (x.strip() for x in line.split('='))
        if verbose:
          print('stat \'{0}\' -> \'{1}\''.format(stat, value))
        stats[sect][strmap[stat]] = float(value)

  if len(stats) == 0:
    return None
  else:
    return stats

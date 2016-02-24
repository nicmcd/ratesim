import os


def getName(filename):
  return os.path.splitext(os.path.basename(filename))[0]


def getFiles(filename, batch, config, code):
  name = getName(filename)
  id = '{0}_{1}'.format(config, code)
  names = {
    'json': filename,
    'id': id,
    'log' : os.path.join(batch, '{0}.log.gz'.format(id)),
    'plot': os.path.join(batch, '{0}.png'.format(id)),
    'data': os.path.join(batch, '{0}.txt'.format(id))}
  return names


def parameters():
  return [
    {
      'code': 'nr',
      'path': 'relays',
      'type': 'uint',
      'affects': ['relay']
    },
    {
      'code': 'mo',
      'path': 'sender_config.max_outstanding',
      'type': 'uint',
      'affects': ['relay']
    },
    {
      'code': 'mt',
      'path': 'sender_config.params.max_tokens',
      'type': 'uint',
      'affects': ['dist1', 'dist2', 'dist3', 'dist4']
    },
    {
      'code': 'st',
      'path': 'sender_config.params.steal_threshold',
      'type': 'float',
      'affects': ['dist2', 'dist3', 'dist4']
    },
    {
      'code': 'tak',
      'path': 'sender_config.params.token_ask_factor',
      'type': 'float',
      'affects': ['dist2', 'dist4']
    },
    {
      'code': 'rak',
      'path': 'sender_config.params.rate_ask_factor',
      'type': 'float',
      'affects': ['dist3', 'dist4']
    },
    {
      'code': 'mro',
      'path': 'sender_config.params.max_requests_outstanding',
      'type': 'uint',
      'affects': ['dist2', 'dist3', 'dist4']
    },
    {
      'code': 'gtt',
      'path': 'sender_config.params.give_token_threshold',
      'type': 'float',
      'affects': ['dist2', 'dist4']
    },
    {
      'code': 'grt',
      'path': 'sender_config.params.give_rate_threshold',
      'type': 'float',
      'affects': ['dist3', 'dist4']
    },
    {
      'code': 'grf',
      'path': 'sender_config.params.give_rate_factor',
      'type': 'float',
      'affects': ['dist3', 'dist4']
    }
  ]

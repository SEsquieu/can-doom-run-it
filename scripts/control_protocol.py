# SPDX-License-Identifier: GPL-3.0-or-later
"""Schema 2 snapshot consumer shared by diagnostics and integration tests."""
import math

MAX_DATAGRAM = 1536

class ControlState:
    def __init__(self):
        self.session = None
        self.sequence = 0
        self.definitions = {}
        self.values = {}
        self.synchronized = False
        self._pending = None
        self._retired = []

    def accept(self, message):
        if message.get('schema') != 2:
            raise ValueError('Unsupported control protocol version')
        session, sequence = message.get('session'), message.get('sequence')
        if not isinstance(session, str) or not session or type(sequence) is not int or sequence < 1:
            raise ValueError('Invalid session or sequence')
        if session != self.session:
            if session in self._retired:
                return False
            retired = (self._retired + ([self.session] if self.session else []))[-8:]
            self.__init__()
            self._retired = retired
            self.session = session
        if sequence <= self.sequence:
            return False
        gap = self.sequence != 0 and sequence != self.sequence + 1
        self.sequence = sequence
        if gap:
            self.synchronized = False
            self._pending = None
        kind = message.get('kind')
        if kind == 'snapshot_begin':
            count = message.get('control_count')
            if type(count) is not int or not 0 <= count <= 32:
                raise ValueError('Invalid snapshot size')
            self._pending = {'id': message['snapshot'], 'count': count, 'definitions': {}, 'values': {}}
        elif kind == 'definition':
            control = message['control']
            if self._pending is not None:
                if message.get('snapshot')!=self._pending['id']:
                    self._pending=None
                    self.synchronized=False
                    return False
                if control not in self._pending['definitions'] and len(self._pending['definitions']) >= self._pending['count']:
                    raise ValueError('Snapshot exceeds declared control count')
                self._pending['definitions'][control] = message
            else:
                if control not in self.definitions and len(self.definitions) >= 32:
                    raise ValueError('Too many controls')
                self.definitions[control] = message
                self.values.setdefault(control, message['neutral'])
        elif kind in ('state', 'event'):
            value = message.get('value')
            if not isinstance(value, list) or len(value) != 2 or not all(type(v) in (int, float) and math.isfinite(v) for v in value):
                raise ValueError('Invalid control value')
            if kind == 'state' and self._pending is not None:
                if message.get('snapshot')!=self._pending['id']:
                    self._pending=None
                    self.synchronized=False
                    return False
                if message['control'] not in self._pending['values'] and len(self._pending['values']) >= self._pending['count']:
                    raise ValueError('Snapshot state exceeds declared count')
                self._pending['values'][message['control']] = value
            elif kind == 'event' and self.synchronized:
                control=message['control']
                if control not in self.definitions:
                    self.synchronized=False
                    return False
                # A trigger is an instantaneous action, never latched state.
                if self.definitions[control].get('type')!='trigger':
                    self.values[control] = value
        elif kind == 'snapshot_end':
            p = self._pending
            if (p is not None and p['id'] == message.get('snapshot') and
                p['count'] == message.get('control_count') and
                len(p['definitions']) == p['count'] and p['definitions'].keys() == p['values'].keys()):
                self.definitions, self.values = p['definitions'], p['values']
                self.synchronized = True
            self._pending = None
        elif kind != 'lifecycle':
            raise ValueError('Unknown message kind')
        return True

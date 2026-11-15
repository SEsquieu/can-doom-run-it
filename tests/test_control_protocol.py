# SPDX-License-Identifier: GPL-3.0-or-later
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from control_protocol import ControlState

def record(sequence, kind, session='launch-a', **fields):
    return dict(schema=2, session=session, sequence=sequence, kind=kind, snapshot=1, **fields)

def snapshot(session='launch-a', start=1, value=0):
    return [record(start,'snapshot_begin',session,control_count=1),
            record(start+1,'definition',session,control='button.a',type='button',neutral=[0,0]),
            record(start+2,'state',session,control='button.a',value=[value,0]),
            record(start+3,'snapshot_end',session,control_count=1)]

class ProtocolTests(unittest.TestCase):
    def test_atomic_snapshot_then_transition(self):
        state=ControlState()
        packets=snapshot(value=1)
        for p in packets[:-1]: state.accept(p)
        self.assertFalse(state.synchronized)
        state.accept(packets[-1])
        self.assertTrue(state.synchronized)
        self.assertEqual(state.values['button.a'],[1,0])
        state.accept(record(5,'event',control='button.a',value=[0,0]))
        self.assertEqual(state.values['button.a'],[0,0])
        self.assertFalse(state.accept(record(5,'event',control='button.a',value=[1,0])))
        self.assertEqual(state.values['button.a'],[0,0])

    def test_gap_needs_resync_and_partial_snapshot_never_commits(self):
        state=ControlState()
        for p in snapshot(): state.accept(p)
        state.accept(record(6,'event',control='button.a',value=[1,0]))
        self.assertFalse(state.synchronized)
        for p in snapshot(start=7,value=1)[::2]: state.accept(p)
        self.assertFalse(state.synchronized)
        for p in snapshot(start=12,value=1): state.accept(p)
        self.assertTrue(state.synchronized)
        self.assertEqual(state.values['button.a'],[1,0])

    def test_restart_is_distinct_even_when_sequence_restarts(self):
        state=ControlState()
        for p in snapshot(value=1): state.accept(p)
        state.accept(record(1,'snapshot_begin','launch-b',control_count=1))
        self.assertFalse(state.synchronized)
        self.assertEqual(state.values,{})
        for p in snapshot('launch-b')[1:]: state.accept(p)
        self.assertTrue(state.synchronized)
        self.assertEqual(state.values['button.a'],[0,0])
        self.assertFalse(state.accept(record(99,'event','launch-a',control='button.a',value=[1,0])))
        self.assertEqual(state.session,'launch-b')

    def test_rejects_unbounded_or_invalid_messages(self):
        state=ControlState()
        with self.assertRaises(ValueError): state.accept(record(1,'snapshot_begin',control_count=33))
        with self.assertRaises(ValueError): state.accept(record(2,'event',control='x',value=[float('nan'),0]))
        with self.assertRaises(ValueError): state.accept(dict(schema=999))

    def test_mixed_snapshot_ids_do_not_commit(self):
        state=ControlState()
        packets=snapshot()
        packets[2]['snapshot']=9
        for p in packets: state.accept(p)
        self.assertFalse(state.synchronized)
        for p in snapshot(start=5): state.accept(p)
        self.assertTrue(state.synchronized)

    def test_unknown_events_cannot_expand_state_and_triggers_do_not_latch(self):
        state=ControlState()
        packets=snapshot()
        packets[1]['type']='trigger'
        for p in packets: state.accept(p)
        state.accept(record(5,'event',control='button.a',value=[1,0]))
        self.assertEqual(state.values['button.a'],[0,0])
        self.assertFalse(state.accept(record(6,'event',control='unknown',value=[1,0])))
        self.assertNotIn('unknown',state.values)
        self.assertFalse(state.synchronized)

if __name__=='__main__': unittest.main()

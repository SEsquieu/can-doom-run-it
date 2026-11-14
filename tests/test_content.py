# SPDX-License-Identifier: GPL-3.0-or-later
"""Reject unplayable room geometry and incomplete distributable content."""
import importlib.util
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('content_builder',ROOT/'scripts/build-content.py')
builder=importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

class ContentTests(unittest.TestCase):
    def test_playable_sector_heights_and_start(self):
        text=(ROOT/'content/maps/DIO01.udmf').read_text()
        sectors=re.findall(r'\bsector\s*\{([^}]+)\}',text)
        self.assertGreater(len(sectors),20)
        for block in sectors:
            fields=dict(re.findall(r'(\w+)\s*=\s*([^;]+);',block))
            # Incorrect field names default to height 0 in the actual engine.
            self.assertGreaterEqual(float(fields['heightceiling'])-float(fields['heightfloor']),128)
        starts=[b for b in re.findall(r'\bthing\s*\{([^}]+)\}',text) if re.search(r'\btype\s*=\s*1;',b)]
        self.assertEqual(len(starts),1)
        ids=set(re.findall(r'user_control\s*=\s*"([^"]+)"',text))
        self.assertEqual(ids,{'trigger.pulse','button.a','button.b','toggle.power','scalar.throttle','vector.pad','selector.mode'})

    def test_original_resources_and_map_archive(self):
        artwork=builder.artwork()
        self.assertTrue(any(n.startswith('sprites/') for n in artwork))
        self.assertTrue(any(n.startswith('textures/') for n in artwork))
        self.assertTrue(all(data.startswith(b'\x89PNG\r\n\x1a\n') for data in artwork.values()))
        wad=builder.wad((ROOT/'content/maps/DIO01.udmf').read_text())
        self.assertEqual(wad[:4],b'PWAD')
        self.assertIn(b'TEXTMAP',wad)
        self.assertIn(b'ENDMAP',wad)

if __name__=='__main__': unittest.main()

"""Atomic strict v1 report, JUnit and a local, escaped artifact index."""
from __future__ import annotations

import html
import json
from pathlib import Path
import xml.etree.ElementTree as ET

from .protocol import ProtocolError, relative_path, validate


def write_json(path, value):
    path = Path(path)
    temporary = path.with_name(path.name + '.pending')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    temporary.replace(path)


def publish(directory, report):
    directory = Path(directory).resolve()
    validate('report', report)
    for case in report['cases']:
        for name in case['artifacts']:
            relative_path(name)
            path = (directory / name).resolve()
            if not path.is_relative_to(directory) or not path.is_file():
                raise ProtocolError('report artifact missing or escaping output: ' + name)
    suite = ET.Element('testsuite', name='demo-differential', tests=str(len(report['cases'])),
                       failures=str(sum(c['verdict'] == 'fail' for c in report['cases'])),
                       errors=str(sum(c['verdict'] == 'error' for c in report['cases']) + len(report['errors'])),
                       skipped=str(sum(c['verdict'] == 'not-run' for c in report['cases'])))
    for case in report['cases']:
        node = ET.SubElement(suite, 'testcase', name=case['identity'], classname='pilot')
        if case['verdict'] != 'pass':
            tag = {'fail': 'failure', 'error': 'error', 'not-run': 'skipped'}[case['verdict']]
            ET.SubElement(node, tag, message=','.join(case['categories'])).text = json.dumps(case['failures'], ensure_ascii=False)
    for error in report['errors']:
        node = ET.SubElement(suite, 'testcase', name='runner.' + error['category'], classname='runner')
        ET.SubElement(node, 'error', message=error['message'])
    # Counts include setup failures as real error test cases, never a green zero-test suite.
    suite.set('tests', str(len(suite)))
    ET.ElementTree(suite).write(directory / 'junit.xml', encoding='utf-8', xml_declaration=True)
    rows = []
    for case in report['cases']:
        links = ' '.join('<a href="' + html.escape(p, quote=True) + '">' + html.escape(p) + '</a>' for p in case['artifacts'])
        rows.append('<tr><td>' + html.escape(case['identity']) + '</td><td>' + case['verdict'] + '</td><td>' +
                    html.escape(case['first_step']) + '</td><td>' + links + '</td></tr>')
    (directory / 'index.html').write_text('<!doctype html><meta charset="utf-8"><title>Differential run</title>' +
        '<h1>' + report['status'] + ' / exit ' + str(report['exit_code']) + '</h1><pre>' +
        html.escape(json.dumps(report['errors'], ensure_ascii=False, indent=2)) + '</pre>' +
        '<table><tr><th>Case</th><th>Verdict</th><th>First divergence</th><th>Artifacts</th></tr>' + ''.join(rows) +
        '</table><p><a href="report.json">Strict report</a> <a href="junit.xml">JUnit</a></p><pre>' +
        html.escape(json.dumps(report['replay_argv'], ensure_ascii=False)) + '</pre>', encoding='utf-8')
    write_json(directory / 'report.json', report)

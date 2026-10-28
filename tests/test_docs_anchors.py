"""Verify that intra-repo Markdown heading links resolve on GitHub and VitePress.

The guides are read both in the repository (GitHub) and on the published VitePress site, but
the two renderers derive heading ids with different slug rules. GitHub drops punctuation and
keeps the resulting hyphen runs, so ``## Freeze / Hold`` becomes ``#freeze--hold``;
VitePress turns punctuation runs into one hyphen, so it becomes ``#freeze-hold``. A link
written for one renderer is dead on the other, which is easy to miss because only the site is
built in CI.

This walks the user-facing Markdown and requires every intra-repo ``#anchor`` link to exist
under both rules. An explicit ``<div id="..."></div>`` anchor counts for both renderers, which
lets a page serve the GitHub-shaped anchor on the site too; GitHub keeps its own heading id.

Run with: python -m unittest discover -s tests -p "test_docs_anchors.py"
"""

from __future__ import annotations

import collections
import os
from pathlib import Path
import re
import unittest
import urllib.parse

REPO_ROOT = Path(__file__).resolve().parents[1]

# Pages that are rendered for readers; build output and task scratch files stay out.
MARKDOWN_ROOTS = ('docs',)
MARKDOWN_FILES = ('README.md', 'CONTRIBUTING.md', 'CHANGELOG.md')
SKIP_DIR_NAMES = {'.vitepress', 'node_modules', 'dist', 'site'}

_FENCE_RE = re.compile(r'^ {0,3}(`{3,}|~{3,})')
_HEADING_RE = re.compile(r'^ {0,3}(#{1,6})[ \t]+(.*?)[ \t]*#*[ \t]*$')
_LINK_RE = re.compile(
    r'\[(?:[^\[\]]|\[[^\]]*\])*\]\(\s*(?P<target><[^>]*>|[^)\s]*)(?:\s+["\'][^)]*["\'])?\s*\)'
)
_HTML_ANCHOR_RE = re.compile(
    r'<(?:a|span|div|p|h[1-6])\b[^>]*?\b(?:id|name)\s*=\s*["\']([^"\']+)["\']', re.IGNORECASE
)
_SCHEME_RE = re.compile(r'^[a-zA-Z][a-zA-Z0-9+.\-]*:')
_EMPHASIS_RE = re.compile(r'(\*\*|__|\*|_)(?=\S)(.+?)(?<=\S)\1')


def fence_free_lines(text):
    """Split Markdown into lines with fenced code blocks removed."""
    lines = []
    fence = None
    for line in text.splitlines():
        match = _FENCE_RE.match(line)
        if match:
            marker = match.group(1)
            if fence is None:
                fence = marker
            elif marker[0] == fence[0] and len(marker) >= len(fence):
                fence = None
            continue
        if fence is None:
            lines.append(line)
    return lines


def plain_text(markdown):
    """Approximate the rendered text of a heading's inline Markdown."""
    text = re.sub(r'!\[([^\]]*)\]\([^)]*\)', r'\1', markdown)  # images
    text = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', text)  # links, keeping their text
    text = re.sub(r'<[^>]+>', '', text)  # raw HTML
    return _EMPHASIS_RE.sub(r'\2', text.replace('`', ''))  # code spans, emphasis


def github_slug(text):
    """github-slugger: lowercase, drop punctuation except '-' and '_', spaces become '-'."""
    return re.sub(r'[^\w\- ]', '', text.strip().lower(), flags=re.UNICODE).replace(' ', '-')


def vitepress_slug(text):
    """@mdit-vue/shared slugify: punctuation and space runs become one '-', then trimmed."""
    text = re.sub(r'[\u0000-\u001f]', '', text)
    text = re.sub(r'[\s~`!@#$%^&*()\-_+=\[\]{}|\\;:\"\'“”‘’<>,.?/]+', '-', text)
    text = re.sub(r'[\u2000-\u206f\u2e00-\u2e7f]', '', text)
    return re.sub(r'-{2,}', '-', text).lower().strip('-')


def page_anchors(path):
    """Return the page's anchors per renderer.

    Both renderers suffix repeated headings with -1, -2, ...; explicit HTML anchors count
    for both renderers because raw HTML passes through unchanged.
    """
    anchors = {'github': set(), 'vitepress': set()}
    seen = {'github': collections.Counter(), 'vitepress': collections.Counter()}
    explicit = []
    for line in fence_free_lines(path.read_text(encoding='utf-8')):
        heading = _HEADING_RE.match(line)
        if heading:
            text = plain_text(heading.group(2))
            for name, slugify in (('github', github_slug), ('vitepress', vitepress_slug)):
                slug = slugify(text)
                if not slug:
                    continue
                occurrence = seen[name][slug]
                seen[name][slug] += 1
                anchors[name].add(slug if occurrence == 0 else '%s-%d' % (slug, occurrence))
        explicit.extend(_HTML_ANCHOR_RE.findall(line))
    for anchor in explicit:
        anchor = anchor.rsplit('#', 1)[-1]
        if anchor:
            for ids in anchors.values():
                ids.add(anchor)
    return anchors


def markdown_files():
    files = [REPO_ROOT / name for name in MARKDOWN_FILES]
    for name in MARKDOWN_ROOTS:
        for path in sorted((REPO_ROOT / name).rglob('*.md')):
            if SKIP_DIR_NAMES.isdisjoint(path.relative_to(REPO_ROOT).parts):
                files.append(path)
    return files


def heading_links(path):
    """Return (line number, href, target page, anchor) for intra-repo anchor links."""
    links = []
    for number, line in enumerate(fence_free_lines(path.read_text(encoding='utf-8')), start=1):
        for match in _LINK_RE.finditer(line):
            href = match.group('target').strip()
            if href.startswith('<') and href.endswith('>'):
                href = href[1:-1].strip()
            if _SCHEME_RE.match(href) or href.startswith('//') or '#' not in href:
                continue
            file_part, _, anchor = href.partition('#')
            anchor = urllib.parse.unquote(anchor)
            if not anchor:
                continue
            if file_part:
                target = Path(os.path.normpath(path.parent / urllib.parse.unquote(file_part)))
            else:
                target = path
            if target.suffix.lower() != '.md':
                continue  # Only Markdown pages have heading anchors on both renderers.
            links.append((number, href, target.resolve(), anchor))
    return links


def display_name(path):
    return path.relative_to(REPO_ROOT).as_posix()


class DocsAnchorTests(unittest.TestCase):
    def test_heading_links_resolve_on_github_and_vitepress(self):
        anchors_by_page = {}
        failures = []
        for source in markdown_files():
            for number, href, target, anchor in heading_links(source):
                if not target.is_file():
                    failures.append('%s:%d: %s (missing page)' % (display_name(source), number, href))
                    continue
                if target not in anchors_by_page:
                    anchors_by_page[target] = page_anchors(target)
                anchors = anchors_by_page[target]
                missing = [name for name in anchors if anchor not in anchors[name]]
                if missing:
                    available = '; '.join(
                        '%s: %s' % (name, ', '.join(sorted(anchors[name]))) for name in missing)
                    failures.append(
                        '%s:%d: %s has no #%s in %s for %s (page has %s)'
                        % (display_name(source), number, href, anchor,
                           display_name(target), ', '.join(missing), available))
        self.assertEqual([], failures, 'Anchor links must work on GitHub and VitePress:\n' + '\n'.join(failures))


if __name__ == '__main__':
    unittest.main()

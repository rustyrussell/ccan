#!/usr/bin/env python

# Copyright (C) 2008 Lukas Lalinsky <lalinsky@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


# CHANGE THIS:
config = {
    'root': '/home/lukas/projects',
    'base_url': '/~lukas/bzrbrowse.cgi',
    'images_url': '/~lukas/bzrbrowse',
    'branch_url': 'http://bzr.oxygene.sk',
}

import os, sys
from bzrlib.branch import Branch
from bzrlib.errors import NotBranchError
from bzrlib import urlutils, osutils

__version__ = '0.0.1'


class HTTPError(Exception):

    def __init__(self, code, message):
        self.code = code
        self.message = message


class NotFound(HTTPError):

    def __init__(self, message):
        super(NotFound, self).__init__('404 Not Found', message)


def escape_html(text):
    return text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;').replace("\n", '<br />')


class BzrBrowse(object):

    icons = {
        'file': 'file.png',
        'directory': 'folder.png',
    }

    page_tmpl = '''<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en"><head><title>%(title)s</title><style type="text/css">
body { font-family: sans-serif; font-size: 10pt; }
div#page { padding: 0.5em; border: solid 1px #444; background: #FAFAFA; }
#footer { font-size: 70%%; background-color: #444; color: #FFF; margin: 0; padding: 0.1em 0.3em; }
#footer hr { display: none; }
h1 { margin: 0; font-size: 14pt; background-color: #444; color: #FFF; padding: 0.1em 0.3em; }
h1 a { color: #FFF; }
h1 a:hover { color: #8CB1D8; }
#page a { color: #244B7C; }
#page a:hover { color: #B12319; }
pre { margin: 0; font-size: 90%%; }
.linenumbers { text-align: right; padding-right: 0.5em; border-right: solid 1px #444; }
.text { padding-left: 0.5em; }
.msg { margin: 0; margin-bottom: 0.5em; padding: 0.3em 0em; border-bottom: solid 1px #444;}
code { background-color: #000; color: #FFF; font-size: 90%%;}
</style>
</head><body>
<h1>%(header)s</h1>
<div id="page">%(contents)s</div>
<div id="footer"><hr />bzrbrowse/%(version)s</div>
</body></html>'''

    def __init__(self, config):
        self.config = config
        self.base_url = None

    def list_to_html(self, entries):
        content = []
        for entry in entries:
            line = '<img src="%(images_url)s/%(icon)s" /> <a href="%(base_url)s/%(path)s">%(name)s</a><br />' % {
                    'base_url': self.config['base_url'],
                    'images_url': self.config['images_url'],
                    'path': entry['path'],
                    'name': entry['name'],
                    'icon': self.icons.get(entry['kind'], self.icons['file'])
                }
            content.append(line)
        return ''.join(content)

    def list_fs_directory(self, path):
        entries = []
        if path:
            entries.append({
                'name': '..',
                'path': os.path.dirname(path),
                'kind': 'directory',
            })
        if path:
            prefix = path + '/'
        else:
            prefix = ''
        try:
            filelist = os.listdir(os.path.join(self.config['root'], path))
        except OSError:
            raise NotFound('Path not found: ' + path)
        for name in sorted(filelist):
            if name.startswith('.'):
                continue
            abspath = os.path.join(path, name)
            if os.path.isdir(os.path.join(self.config['root'], abspath)):
                entries.append({
                    'name': name,
                    'path': prefix + name,
                    'kind': 'directory',
                })
        return self.list_to_html(entries)

    def view_branch_file(self, tree, ie):
        if ie.text_size > 1024 * 1024:
            return 'File too big. (%d bytes)' % (ie.text_size)
        tree.lock_read()
        try:
            text = tree.get_file_text(ie.file_id)
        finally:
            tree.unlock()
        if '\0' in text:
            return 'Binary file. (%d bytes)' % (ie.text_size)
        try:
            text = text.decode('utf-8')
        except UnicodeDecodeError:
            text = text.decode('latin-1')
        linenumbers = []
        for i in range(1, text.count('\n') + 1):
            linenumbers.append('<a id="l-%d" href="#l-%d">%d</a>' % (i, i, i))
        linenumbers = '\n'.join(linenumbers)
        return ('<table cellspacing="0" cellpadding="0"><tr><td class="linenumbers"><pre>' +
                linenumbers + '</pre></td><td class="text"><pre>' + escape_html(text) +
                '</pre></td></tr></table>')

    def list_branch_directory(self, branch, path, relpath):
        tree = branch.basis_tree()
        file_id = tree.path2id(relpath)
        ie = tree.inventory[file_id]
        if ie.kind == 'file':
            return self.view_branch_file(tree, ie)
        entries = []
        if path:
            entries.append({
                'name': '..',
                'path': urlutils.dirname(path),
                'kind': 'directory',
            })
        if path:
            prefix = path + '/'
        else:
            prefix = ''
        for name, child in sorted(ie.children.iteritems()):
            entries.append({
                'name': name,
                'path': prefix + name,
                'kind': child.kind,
            })
        html = self.list_to_html(entries)
        base = self.config['branch_url'] + '/' + osutils.relpath(self.config['root'], urlutils.local_path_from_url(branch.base))
        html = ('<p class="msg">This is a <a href="http://bazaar-vcs.org/">Bazaar</a> branch. ' +
                'Use <code>bzr branch ' + base + '</code> to download it.</p>' + html)
        return html

    def request(self, path):
        abspath = os.path.join(self.config['root'], path)
        try:
            branch, relpath = Branch.open_containing(abspath)
        except NotBranchError:
            return self.list_fs_directory(path)
        return self.list_branch_directory(branch, path, relpath)

    def title(self, path):
        return '/' + path

    def header(self, path):
        title = []
        p = ''
        title.append('<a href="%s%s">root</a>' % (self.config['base_url'], p))
        for name in path.split('/'):
            p += '/' + name
            title.append('<a href="%s%s">%s</a>' % (self.config['base_url'], p, name))
        return '/'.join(title)

    def __call__(self, environ, start_response):
        try:
            path = '/'.join(filter(bool, environ.get('PATH_INFO', '').split('/')))
            contents = self.page_tmpl % {
                'title': self.title(path),
                'header': self.header(path),
                'contents': self.request(path),
                'version': __version__
            }
            contents = contents.encode('utf-8')
            headers = [('Content-type','text/html; charset=UTF-8')]
            start_response('200 OK', headers)
            return [contents]
        except HTTPError, e:
            headers = [('Content-type','text/html; charset=UTF-8')]
            start_response(e.code, headers)
            return [e.message]
        except:
            import cgitb, sys
            traceback_html = cgitb.html(sys.exc_info())
            headers = [('Content-type','text/html; charset=UTF-8')]
            start_response('200 OK', headers)
            return [traceback_html]


from wsgiref.handlers import CGIHandler
CGIHandler().run(BzrBrowse(config))

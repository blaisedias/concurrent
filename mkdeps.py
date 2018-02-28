#!/usr/bin/python2
import os
import os.path
import sys
import re

from optparse import OptionParser

re_c_file = re.compile("^..*\.(c|cpp|cxx)$", re.I)
re_hdr_file = re.compile("^..*\.(h|hpp|hxx)$", re.I)
re_src_files = re.compile("^..*\.(c|cpp|cxx|h|hpp|hxx)$", re.I)
re_include = re.compile('^#include\s*"(?P<incf>.*)"$')

def scan(dpath, deps):
    for (path, dirs, files) in os.walk(dpath):
        files.sort()
        for f in files:
            if re_src_files.match(f):
                srcpath = "%s/%s" % (path, f)
                deps[srcpath] = []
                with open(srcpath) as fp:
                    lines = fp.readlines()
                for line in lines:
                    line = line.strip()
                    m = re_include.match(line)
                    if m:
                        try:
                            deps[srcpath].append(m.groupdict()['incf'])
                        except KeyError:
                            deps[srcpath] = [m.groupdict()['incf']]

def fixIncDep(inc, deps):
    # FIXME: will not handle multiplicate file names in different trees correctly.
    if inc in deps.keys():
        return inc
    rv = []

    for path in deps.keys():
        pcomps = path.split('/')
        icomps = inc.split('/')
        while(len(pcomps) > len(icomps)):
            pcomps.pop(0)
        if pcomps == icomps:
            rv.append(path)

    if len(rv) == 1:
        return rv[0]

    if len(rv) > 1:
        raise RuntimeError("FIXME: Too dumb! multiple files matching include spec")

    raise RuntimeError("No file found matching {}".format(inc))


def gen_deps1(output, regex, deps):
    for path, incs in sorted(deps.iteritems()):
        if len(incs) and regex.search(path):
            output.append('{} : {}'.format(path, ' '.join(sorted(incs))))
            output.append('')

def gen_deps2(output, regex, deps):
    for path, incs in sorted(deps.iteritems()):
        if len(incs) and regex.search(path):
            pdeps = [inc for inc in incs]
            for inc in incs:
                pdeps.extend(deps[inc])
            output.append('{} : {}'.format(path, ' '.join(sorted(pdeps))))
            output.append('')

def gen_deps3(output, regex, deps, objdir=''):
    for path, incs in sorted(deps.iteritems()):
        if len(incs) and regex.search(path):
            obj = '{}{}.o'.format(objdir, path.split('/')[-1].split('.')[0])
            pdeps = [inc for inc in incs]
            for inc in incs:
                pdeps.extend(deps[inc])
            output.append('{} : {} {}'.format(obj, path, ' '.join(sorted(pdeps))))
            output.append('')

parser = OptionParser()
parser.add_option("-v", "--verbose", dest="verbose", action="store_true", default=False)
parser.add_option("-o", "--objdir", dest="objdir", default="")
parser.add_option("-f", "--file", dest="makefiledeps", action="store", default="Makefile.deps")
(options, args) = parser.parse_args()
if len(options.objdir) and options.objdir[-1] != '/':
    options.objdir = '{}/'.format(options.objdir)

deps = {}
for arg in args:
    scan(arg, deps)

for path, incs in sorted(deps.iteritems()):
    for i in range(len(incs)):
        incs[i] = fixIncDep(incs[i], deps)

lines = []
#lines.append("#--------------- src file dependencies")
#gen_deps1(lines, re_c_file, deps)
#lines.append("#--------------- src file dependencies concatenated")
#gen_deps2(lines, re_c_file, deps)
#lines.append("#--------------- obj file dependencies concatenated")
gen_deps3(lines, re_c_file, deps, options.objdir)

try:
    with open(options.makefiledeps, 'rb') as fp:
        curr_lines = [x.strip() for x in fp.readlines()]
except Exception:
    curr_lines = []


if lines != curr_lines:
    with open(options.makefiledeps, 'wb') as fp:
        for line in lines:
            fp.write('{}\n'.format(line))
    print >> sys.stderr, "{} file is updated.".format(options.makefiledeps)
else:
    print >> sys.stderr, "{} file is upto date.".format(options.makefiledeps)

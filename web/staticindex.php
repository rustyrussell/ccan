<?php
session_start();
include('logo.html');
include('menulist.html');
include('static-configuration');
?>
<div class='content'>
<h2>The Idea</h2>

<p>
That nice snippets of C code should be moved out of junkcode
directories and exposed to a wider world, where they can become
something useful.
</p>

<p>
CCAN is loosely modelled after the successful <a href="http://cpan.org">CPAN project</a>
for Perl code development and sharing.
</p>

<h2>Get The Code</h2>

<p> You can get each module as a tarball (<a href="list.html">see
list</a>), get a tarball of <a href="ccan.tar.bz2">the whole repository</a> with tools,
or clone our <a href="http://git.ozlabs.org/?p=ccan">git repository</a> (<tt>git clone git://git.ozlabs.org/~ccan/ccan</tt>) or the one on <a href="http://github.com/rustyrussell/ccan/">github</a>.
</p>

<h2>Use The Code</h2>
<p>
There are two ways to use it:
<ol>
<li> Put modules into a ccan/ subdirectory into your project.  Add a "config.h" (like
     <a href="example-config.h">this example</a>, or generate one using <a href="http://git.ozlabs.org/?p=ccan;a=blob_plain;f=tools/configurator/configurator.c">the configurator</a>) and compile every .c file
     in ccan/* (as in this <a href="Makefile-ccan">Makefile</a>)).

<li> Alternatively, just hack whatever parts you want so it compiles in
your project.
</ol>
</p>

<h2>Add Code</h2>
<p>
We always welcome new code; see <a href="http://github.com/rustyrussell/ccan/wiki/Contribute">how!</a>.
</p>

<p>Anyone can send code or a git pull request to
the <a href="http://ozlabs.org/mailman/listinfo/ccan">friendly
mailing list</a> or just <a href="upload.html">upload it using the web form</a>.
</p>

<p>
"GPLv2 or later" and supersets thereof (eg. LGPLv2+ or BSD)
licenses preferred.
</p>

<h2>Complaints, Ideas and Infrastructure</h2>

<p>
We have a <a href="http://ozlabs.org/mailman/listinfo/ccan">low volume
mailing list</a> for discussion of CCAN in general, and you're welcome
to join.
</p>

<p>
We also have an IRC channel: #ccan on <a href="http://freenode.net">Freenode</a>.
</p>

<p>
We also have a <a href="http://github.com/rustyrussell/ccan/wiki/">wiki</a>; feel free to enhance it.
</p>
</div>
</body></html>

<?php
session_start();
include('logo.html');
include('menulist.html');
include('static-configuration');
?>

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

<h2>Getting The Code</h2>

<p>
Once you <a href="list.html">grab some modules</a>, there are two basic
ways to use it:
<ol>
<li> Just hack it to compile in your project.

<li> Use it in place by giving it a "config.h" (steal
     <a href="example-config.h">this example</a>) and compiling all the .c
       files (simply, or as in this <a href="Makefile-ccan">Makefile</a>).
</ol>
</p>

<p>
There's also a
<a href="http://bazaar-vcs.org/">Bazaar</a> repository for all the CCAN
infrastructure at http://ccan.ozlabs.org/repo (<a href="http://ccan.ozlabs.org/browse">browse</a>).
</p>

<h2>Module Contributions</h2>

<p>
We welcome new code!  The guide to creating new modules is a work in
progress (just copy an existing module), but anyone can send code to
the <a href="http://ozlabs.org/mailman/listinfo/ccan">friendly
mailing list</a> or just <a href="upload.html">upload it</a>.
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
We also have a <a href="Wiki/">wiki</a>; feel free to enhance it.
</p>

<p>
<i>Rusty Russell</i>
</p>

<hr>
</body></html>

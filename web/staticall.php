<?php
session_start();
include('logo.html');
include('menulist.html');
include('static-configuration');

$tarballsize=round((filesize($argv[3]."/ccan.tar.bz2") + 1023) / 1024);
?>
<div class='content'>
<h1> List of all CCAN modules: </h1>

<p>
Note that two downloads are offered: the first includes with all the
other ccan modules this module uses, the second is a standalone
download.
</p>

<p>
Or you can just download the <a href="ccan.tar.bz2">tarball of everything including CCAN tools (<?=$tarballsize?>K)</a>.
</p>

<table align="center" cellpadding="3" cellspacing="1">
<th align="left">Name</th>
<th align="left">Summary / Link to details</th>
<th align="right">Download</th>

<?php 
$modules = array_slice($argv, 4);
sort($modules);

foreach ($modules as $module) {
	$summary=extract_field('summary',$argv[1].$module);
	$with_deps="$tar_dir/with-deps/$module.tar.bz2";
	$no_deps="$tar_dir/$module.tar.bz2";
	$with_dep_size = round((filesize($argv[3]."/".$with_deps) + 1023) / 1024);
	$no_dep_size = round((filesize($argv[3]."/".$no_deps) + 1023) / 1024);
	?>
	<tr>
	  <td><?=$module?></td>
	  <td><a href="info/<?=$module?>.html"><?=$summary?></a></td>
	  <td align="right"><a href="<?=$with_deps?>"><?=$with_dep_size?>K</a> / 
	      <a href="<?=$no_deps?>"><?=$no_dep_size?>K</a></td>
	</tr>
	<?php
}
?>
</table>

<h2> Contents of Junkcode: </h2>

(This is contributed code which was dumped here: these gems may need some polishing.)

<table align="center" cellpadding="3" cellspacing="1">

<?php
$d = dir($argv[2]);
$dirs = array();
while (false !== ($entry = $d->read())) {
	if ($entry[0] != '.') {
		array_push($dirs, $entry);
	}
}

sort($dirs);
foreach ($dirs as $dir) {
	$size = round((filesize($argv[3]."/junkcode/".$dir.".tar.bz2") + 1023) / 1024);
	echo "<tr><td><a href=\"junkcode/$dir.tar.bz2\">$dir.tar.bz2 (${size}K)</a></td>\n";
	echo "<td><a href=\"junkcode/$dir.html\">Browse contents...</a></td><tr>\n";
}
$d->close();
?>
</table>
</div>
</body></html>

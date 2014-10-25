<?php
session_start();
include('logo.html');
include('menulist.html');
include('static-configuration');
?>
<div class='content'>
<h1> Contents of CCAN <?=$argv[1]?> </h1>
<p>

<table align="center" width="80%" border="0" cellpadding="3" cellspacing="1">
<?php 
$d = dir($argv[1]);
$files = array();
while (false !== ($entry = $d->read())) {
	if ($entry[0] != '.') {
		array_push($files, $entry);
	}
}
$d->close();
sort($files);

foreach ($files as $file) {
	$size = round((filesize($argv[1]."/".$file) + 1023) / 1024);
	?>
	<tr>
	  <td><a href="<?=$argv[2]."/".$file?>"><?=$file?> (<?=$size?>K)</td>
	</tr>
	<?php
}
?>
</table>
</div>
</body></html>

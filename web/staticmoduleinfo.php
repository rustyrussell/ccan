<?php
session_start();
include('logo.html');
include('menulist.html');
include('static-configuration');
$module_path=$argv[1];
$module=$argv[2];
$maintainer=extract_field('maintainer',$module_path);
$author=extract_field('author',$module_path);
$summary=extract_field('summary',$module_path);
$see_also=extract_field('see_also',$module_path);
$description=htmlize_field('description',$module_path);
$example=extract_field('example',$module_path);
$dependencies=htmlspecialchars(shell_exec('tools/ccan_depends --direct '.$module_path));
$extdepends=htmlspecialchars(shell_exec('tools/ccan_depends --compile --non-ccan '.$module_path));
$licence=extract_field('licence',$module_path);
$license=extract_field('license',$module_path);
$url_prefix = getenv("URLPREFIX");
?>
<table align="center" bgcolor="lightblue" width="70%" border="0" cellpadding="3" cellspacing="1">
<tr align="center" bgcolor="FFFFCC">
<td>
<a href="<?=$repo_base.$module?>">Browse Source</a>
</td>
<td>
<a href="<?=$url_prefix?><?=$tar_dir?>/with-deps/<?=$module?>.tar.bz2">Download</a>
<a href="<?=$url_prefix?><?=$tar_dir?>/<?=$module?>.tar.bz2">(without any required ccan dependencies)</a>
</tr>
</table>

<p>
</p>

<table align="center" bgcolor="lightblue" width="70%" border="0" cellpadding="8" cellspacing="1">
<tr align="left" bgcolor="FFFFCC">
<td><h3>Module: </h3> <?=$module?> </td>
</tr>

<tr align="left" bgcolor="FFFFCC">
<td><h3>Summary: </h3> <?=$summary?></td>
</tr>

<?php
if ($maintainer) {
?>
<tr align="left" bgcolor="FFFFCC"> 
<td><h3>Maintainer: </h3> <?=$maintainer?></td>
</tr>
<?php
}

if ($author) {
?>
<tr align="left" bgcolor="FFFFCC"> 
<td><h3>Author: </h3> <?=$author?></td>
</tr>
<?php
}

if ($dependencies) {
?>
<tr align="left" bgcolor="FFFFCC">
<td><h3>Dependencies: </h3> <pre> <?php
	foreach (preg_split("/\s+/", $dependencies) as $dep) {
		echo '<a href="'.substr($dep, 5).'.html">'.$dep.'</a> ';
        }
?></pre></td>
</tr>
<?php 
}

if ($extdepends) {
?>
<tr align="left" bgcolor="FFFFCC">
<td><h3>External dependencies: </h3> <?php
	foreach (split("\n", $extdepends) as $dep) {
		$fields=preg_split("/\s+/", $dep);
		echo $fields[0].' ';
		if (count($fields) > 1)
			echo '(version '.$fields[1].') ';
		echo '<br>';
        }
?></td>
</tr>
<?php 
}
?>
<tr align="left" bgcolor="FFFFCC">
<td><h3>Description: </h3> <?=$description;?> </td>
</tr>

<?php 
if ($see_also) {
?>
<tr align="left" bgcolor="FFFFCC">
<td><h3>See Also: </h3> <pre> <?php
	foreach (preg_split("/[\s,]+/", trim($see_also)) as $see) {
		echo '<a href="'.substr($see, 5).'.html">'.$see.'</a> ';
        }
?></pre></td>
</tr>
<?php
}

if ($example) {
?>
<tr align="left" bgcolor="FFFFCC"> 
<td><h3>Example: </h3> <pre><?=$example?></pre></td>
</tr>
<?php
}

if ($licence) {
?>
<tr align="left" bgcolor="FFFFCC"> 
<td><h3>License: </h3> <?=$licence?></td>
</tr>
<?php
}

if ($license) {
?>
<tr align="left" bgcolor="FFFFCC"> 
<td><h3>License: </h3> <?=$license?></td>
</tr>
<?php
}
?>
</table><hr>
</body></html>

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
<div class='content moduleinfo'>

<p><a href="<?=$repo_base.$module?>">Browse Source</a> <a href="<?=$url_prefix?><?=$tar_dir?>/with-deps/<?=$module?>.tar.bz2">Download</a> <a href="<?=$url_prefix?><?=$tar_dir?>/<?=$module?>.tar.bz2">(without any required ccan dependencies)</a></p>

<h3>Module:</h3>
<p><?=$module?></p>

<h3>Summary:</h3>
<p><?=$summary?></p>

<?php
if ($maintainer) {
?>
<h3>Maintainer:</h3>
<p><?=$maintainer?></p>
<?php
}

if ($author) {
?>
<h3>Author:</h3>
<p><?=$author?></p>
<?php
}

if ($dependencies) {
?>
<h3>Dependencies:</h3>
<ul class='dependencies'>
<?php
	foreach (preg_split("/\s+/", $dependencies) as $dep) {
		if ($dep) {
			echo '<li><a href="'.substr($dep, 5).'.html">'.$dep.'</a></li>';
		}
        }
?>
</ul>

<?php 
}

if ($extdepends) {
?>
<h3>External dependencies:</h3>
<ul class='external-dependencies'>
<?php
	foreach (split("\n", $extdepends) as $dep) {
		$fields=preg_split("/\s+/", $dep);
		echo "<li>" . $fields[0].' ';
		if (count($fields) > 1)
			echo '(version '.$fields[1].') ';
		echo '</li>';
        }
?>
</ul>
<?php 
}
?>

<h3>Description:</h3>
<p><?=$description;?></p>

<?php 
if ($see_also) {
?>
<h3>See Also:</h3>
<ul class='see-also'>
<?php
	foreach (preg_split("/[\s,]+/", trim($see_also)) as $see) {
		echo '<li><a href="'.substr($see, 5).'.html">'.$see.'</a></li>';
        }
?>
</ul>
<?php
}

if ($example) {
?>
<h3>Example:</h3>
<pre class="prettyprint">
<code class="language-c"><?=$example?></code>
</pre>
<?php
}

if ($license) {
?>
<h3>License:</h3>
<p><?=$license?></p>
<?php
}
?>
</div>
</body></html>

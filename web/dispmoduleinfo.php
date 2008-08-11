<?php
session_start();
include('logo.html');
include('menulist.html');
include('configuration');
include('search.html');
$handle = sqlite3_open($db) or die("Could not open database");
$query = "select * from search where module=\"".$_GET['module']."\"";
$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));
$row = sqlite3_fetch_array($result);

if (!file_exists($repopath . $ccan_home_dir.$_GET['module']."/".$_GET['module'].".tar")) {
	chdir($repopath);
	exec("tar -cvvf ".$ccan_home_dir. $_GET['module']. "/". $_GET['module'].".tar ". $ccan_home_dir.$_GET['module'], $status);
	chdir("..");
}

if (!file_exists($repopath . $ccan_home_dir.$_GET['module']."/".$_GET['module']."_dep.tar")) {
	chdir($repopath);
	exec($create_dep_tar." ".$ccan_home_dir.$_GET['module'], $status);
	chdir("..");
}


?>
<table align="center" bgcolor="lightblue" width="70%" border="0" cellpadding="3" cellspacing="1">
<tr align="center" bgcolor="FFFFCC">
<td width="50%"><a href=<?=$repopath . $ccan_home_dir.$_GET['module']?>/<?=$_GET['module']?>.tar>Download</a></td>
<td><a href=<?=$repopath . $ccan_home_dir.$_GET['module']?>/<?=$_GET['module']?>_dep.tar>Download Dependencies</a></td>
</tr>
</table>
<table align="center" bgcolor="lightblue" width="70%" border="0" cellpadding="8" cellspacing="1">
<tr align="left" bgcolor="FFFFCC">
<td><h3>Module: </h3> <pre><?=$row['module'];?></pre> </td>
</tr>

<tr align="left" bgcolor="FFFFCC">
<td><h3>Title: </h3> <pre><?=$row['title'];?> </pre></td>
</tr>
 		
<tr align="left" bgcolor="FFFFCC"> 
<td><h3>Author: </h3> <pre><a href=search.php?author=<?=$row['author'];?>><?=$row['author'];?></a></pre></td>
</tr>

<tr align="left" bgcolor="FFFFCC">
<td><h3>Description: </h3> <pre><?=$row['desc'];?></pre></td>
</tr>
</table><hr>

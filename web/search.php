<?php
session_start();
include('logo.html');
include('menulist.html');
include('configuration');
include('searchengine.php');
include('search.html');

if(isset($_POST['search'])) {
	$searchtext = $_REQUEST['searchtext'];
	$in = $_REQUEST['searchmenu'];
	if(trim($searchtext) == '') { 
		echo '<div align="center"><font color="RED">Please enter some keyword to search</font></div>';
		exit();
	}
}
else if($_GET['author'] != '') {
	$searchtext = $_GET['author'];
	$in = "author";
}	
else if ($_GET['disp'] == 'all') {
	$searchtext = "";
	$in = "module";
}	
else 
	exit();
	
$result = searchdb($searchtext, $in, $db);
echo '<table align="left" border="0" cellpadding="8" cellspacing="1">';
if($row = sqlite3_fetch_array($result)) 
	echo "<tr><td><a href=\"dispmoduleinfo.php?module=".$row['module']."\">".$row["module"]."</a></br>".
		 "<a href=\"search.php?author=".$row["author"]."\">".$row["author"]."</a> : ". $row["title"]." </br> </br></td></tr>";
else
	echo '<div align="center"><font color="RED"> No results found</font></div>';
while($row = sqlite3_fetch_array($result)) {	
	echo "<tr><td><a href=\"dispmoduleinfo.php?module=".$row['module']."\">".$row["module"]."</a></br>".
		 "<a href=\"search.php?author=".$row["author"]."\">".$row["author"]."</a> : ". $row["title"]." </br> </br></td></tr>";
}
echo '</table>';
?>

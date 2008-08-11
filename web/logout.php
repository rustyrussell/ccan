<?php
session_start();

include('logo.html');
include('menulist.html');

if($_SESSION['slogged'] != '')
	echo "<br><div align=\"center\">Logged out Successfully...</div>";
else
   echo "<br><div align=\"center\">Please login...</div>";

session_destroy();
?>
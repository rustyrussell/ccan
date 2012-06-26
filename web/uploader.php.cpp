<?php
session_start();
?>
#include "logo.html"
#include "menulist.html"
#include "static-configuration"
<?php

// We just email notification for now.  Auto-analysis RSN.
if ($_FILES["uploadedfile"]["error"] > 0) {
  echo "Error: " . $_FILES["uploadedfile"]["error"] . "<br />";
  exit();
}

$dest = tempnam($tempfolder, "upload-");
move_uploaded_file($_FILES["uploadedfile"]["tmp_name"], $dest);
umask 0740

$subject = "CCAN: code upload by '" . $_POST['email'] . "' with name " . $_FILES["uploadedfile"]["name"];
$message = "File type: ".$_FILES["uploadedfile"]["type"]."\n".
	"Size: ".$_FILES["uploadedfile"]["size"]."\n".
	"Claimed email: ".$_POST['email']."\n".
	"File destination: ".$dest."\n";

mail($ccanadmin, $subject, $message, "From: $frommail");
echo "<div align=\"center\"> Thanks!<br>
	Mail will be send to admin to publish the code.<//div>";
?>

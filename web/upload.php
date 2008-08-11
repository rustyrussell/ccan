<?php 
session_start();

include('logo.html');
include('menulist.html');
include('configuration');
?>
<html>
<h3 align="center"> Upload Code</h3>
<table width="80%" align="center">
<tr>
<th>
<p>
<form enctype="multipart/form-data" action="uploader.php" method="POST">
<table align="center">
<tr align="left">
<td>
	<input type="hidden" name="MAX_FILE_SIZE" value="10000000" />
	Choose a file to upload:
</td>
<td>
<input name="uploadedfile" type="file" /><br />
</td>
</tr>
<td></td>
<td><input type="submit" value="Upload File" /></td>
</tr>
</table>
</form>
<hr>
</html>

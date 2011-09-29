<?php 
session_start();

include('logo.html');
include('menulist.html');
include('static-configuration');
?>
<html>
<h3 align="center"> Upload Code</h3>

<table width="70%" align="center">
<tr>
<td>
<p>
Got C code sitting around which might help someone?  Put it to work
by uploading here; .tar.gz, .zip or even single C files.
</p>

<p>If it has a valid _info file and a testsuite (see <a href="http://ccodearchive.net/Wiki/ModuleGuide">the module creation guide</a>), it'll go into the
main repository.  Otherwise, it'll go into our "junkcode" area where
people can browse and download it.
</p>
</td>
</tr>
</table>

<table width="70%" align="center">
<tr>
<p>
<form enctype="multipart/form-data" action="<?=$uploadscript?>" method="POST">
<td align="right">
	Email address:
</td>
<td>
<input type="text" name="email" size="25"
	     maxlength="255" value="" />
</td>
</tr>
<tr>
<td align="right">
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

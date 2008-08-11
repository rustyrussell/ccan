<?php
session_start(); // start session.
if($_SESSION["slogged"] == false) {
	header('Location: login.php?referer=changedetail.php');
	exit();
}

else {
include('logo.html');
include('menulist.html');
include('configuration');

//get account data 
$handle = sqlite3_open($db) or die("Could not open database");
$accountid = $_SESSION['susername'];
$query = "SELECT * FROM users where username=\"$accountid\"";
$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));

$row = sqlite3_fetch_array($result);
$name = $row["name"];
$email = $row["email"];
$website = $row["website"];
$password = '';
$repassword = '';
}

if(isset($_POST['submit'])) {
	$name = $_REQUEST['name'];
	$email = $_REQUEST['email'];
	$website = $_REQUEST['website'];
	$password = $_REQUEST['password'];
	$repassword = $_REQUEST['repassword'];
	if(trim($name) == '') { 
		$errmsg = 'Please enter your name';
	} 

	else if(trim($email) == '') {
		$errmsg = 'Please enter your email address';
	}

	else if(!isEmail($email)) {
		$errmsg = 'Your email address is not valid';
	}

	if($password != '' || $repassword != '') {
		if(strlen($password) < 6 || strlen($password) > 16) 
			 $errmsg = 'Password should have length between 6 and 16';
		if($password != $repassword)
			 $errmsg = 'Password and retype password not match';
	}
}

if(trim($errmsg) != '' || !isset($_POST['submit'])) {
?>
		<h3 class="firstheader" align="center">Change CCAN account</h3>
		<div align="center" class="errmsg"><font color="RED"><?=$errmsg;?></font></div>
		<div align="center">Note: Please leave password fields blank if you donot want to change</div>
		<form method="post" align="center" action="changedetail.php">
		<table align="center" width="70%" border="0" bgcolor="gray" cellpadding="8" cellspacing="1">
 		<tr align="left" bgcolor="lightgray">
 		<td><p>Full name: </p><p><input name="name" type="text" value="<?=$name;?>"/></p></td
		</tr>
 		<tr align="left" bgcolor="silver">
 		<td><p>Email: </p><p><input name="email" type="text" value="<?=$email;?>"/> </p></td>
		</tr>
 		<tr align="left" bgcolor="lightgray">
 		<td><p>New Password: </p><p><input name="password" type="password" value="<?=$password;?>"/></p></td>
		</tr>
 		<tr align="left" bgcolor="silver">
 		<td><p>Retype Password: </p><p><input name="repassword" type="password" value="<?=$repassword;?>"/><br /></p>
		</td>
		</tr>
		<tr align="left" bgcolor="lightgray">
 		<td><p>Web Site[Optional]: </p><p><input name="website" type="text" value="<?=$website;?>"/><br /></p>
		</td>
		</tr>
 		<tr align="center">
 		<td><input type="submit" name="submit" value="Change Account"/></td>
		</tr>
		</table>
		</form>
		<hr>
		</body>
		</html>
<?php
}
else {
$handle = sqlite3_open($db) or die("Could not open database");
$query = "update users set name=\"".$name."\",email=\"".$email."\",website=\"".$website."\" where username=\"$accountid\"";
$result = sqlite3_exec($handle, $query) or die("Error in query: ".sqlite3_error($handle));
$ispass = '';
if($password != '' && $repassword != '' && $password == $repassword ) {
	$password = md5($password);
	$query = "update login set password=\"$password\" where username=\"$accountid\"";
	$result = sqlite3_exec($handle, $query) or die("Error in query: ".sqlite3_error($handle));
	$ispass = "password. Please login again";
	session_destroy();
}
echo "<div align=\"center\"> Sucessfully changed ".$ispass."... <//div><//body><//html>";
}

function isEmail($email)
{
	return(preg_match("/^[-_.[:alnum:]]+@((([[:alnum:]]|[[:alnum:]][[:alnum:]-]*[[:alnum:]])\.)+(ad|ae|aero|af|ag|ai|al|am|an|ao|aq|ar|arpa|as|at|au|aw|az|ba|bb|bd|be|bf|bg|bh|bi|biz|bj|bm|bn|bo|br|bs|bt|bv|bw|by|bz|ca|cc|cd|cf|cg|ch|ci|ck|cl|cm|cn|co|com|coop|cr|cs|cu|cv|cx|cy|cz|de|dj|dk|dm|do|dz|ec|edu|ee|eg|eh|er|es|et|eu|fi|fj|fk|fm|fo|fr|ga|gb|gd|ge|gf|gh|gi|gl|gm|gn|gov|gp|gq|gr|gs|gt|gu|gw|gy|hk|hm|hn|hr|ht|hu|id|ie|il|in|info|int|io|iq|ir|is|it|jm|jo|jp|ke|kg|kh|ki|km|kn|kp|kr|kw|ky|kz|la|lb|lc|li|lk|lr|ls|lt|lu|lv|ly|ma|mc|md|mg|mh|mil|mk|ml|mm|mn|mo|mp|mq|mr|ms|mt|mu|museum|mv|mw|mx|my|mz|na|name|nc|ne|net|nf|ng|ni|nl|no|np|nr|nt|nu|nz|om|org|pa|pe|pf|pg|ph|pk|pl|pm|pn|pr|pro|ps|pt|pw|py|qa|re|ro|ru|rw|sa|sb|sc|sd|se|sg|sh|si|sj|sk|sl|sm|sn|so|sr|st|su|sv|sy|sz|tc|td|tf|tg|th|tj|tk|tm|tn|to|tp|tr|tt|tv|tw|tz|ua|ug|uk|um|us|uy|uz|va|vc|ve|vg|vi|vn|vu|wf|ws|ye|yt|yu|za|zm|zw)$|(([0-9][0-9]?|[0-1][0-9][0-9]|[2][0-4][0-9]|[2][5][0-5])\.){3}([0-9][0-9]?|[0-1][0-9][0-9]|[2][0-4][0-9]|[2][5][0-5]))$/i"
			,$email));
}
?>

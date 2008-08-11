<?php
session_start(); // start session.
include('configuration');

if($_SESSION['slogged'] != ''){
	include('logo.html');
	include('menulist.html');
	echo "<br><div align=\"center\">Already logged in as ".$_SESSION['susername']."...</div>";
	exit();
}

if(!isset($_POST['submit'])) {
	include('logo.html');
	include('menulist.html');
	loginhtml("Members only. Please login to access.");
	exit();
}

// get username and password
$username = $_POST['username'];
$password = $_POST['password'];

// register username and logged as session variables.
session_register("susername");
session_register("slogged"); 

//set session variables
$_SESSION["susername"] = $username;
$_SESSION["slogged"] = false;

// open database file
$handle = sqlite3_open($db) or die("Could not open database");
// query string
$query = "SELECT * FROM login where username=\"$username\"";
// execute query
$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));
// if rows exist
if (($row = sqlite3_fetch_array($result)) != '') { 
	if(md5($password) == $row["password"])
		$valid_user = 1;
}
else {
$valid_user = 0;
}

//if not valid user
if (!($valid_user)) {
	// Unset session variables.
	session_unset();   
	include('logo.html');
	include('menulist.html');
	loginhtml("Incorrect login information, please try again. You must login to access.");
	exit();
}

//if valid user
else {
	$referer = $_GET['referer'];
	$_SESSION["slogged"] = true;
	if($referer != '') {
		header('Location: '.$referer);
		exit();
	}	
	include('logo.html');
	include('menulist.html');
	echo "<br><div align=\"center\">Logged in sucessfully...<//div><//body><//html>";
}



function loginhtml($info)
{
?>
<form action="<?=$PHP_SELF.$referer?><?if($QUERY_STRING){ echo"?". $QUERY_STRING;}?>" method="POST">
<p align="center"><?=$info?></p>
<table align="center" border="0">
 <tr>
  <th>
Username:
  </th>
  <th>
<input type="text" name="username">
  </th>
 </tr>
 <tr>
  <th>
Password:
  </th>
  <th>
<input type="password" name="password">
  </th>
 </tr>
 <tr>
  <th colspan="2" align="right">
<input type="submit" name="submit" value="Login">
</form>
  </th>
 </tr>
</table><hr>
</body>
</html>
<?php
}
?>


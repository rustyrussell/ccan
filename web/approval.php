<?php
session_start();
if($_SESSION["slogged"] == false) {
	header('Location: login.php?referer=approval.php?accountid='.$_GET['accountid']);
	exit();
}

include('logo.html');
include('menulist.html');
include('configuration');
$accountid = $_GET['accountid'];
$username = $_SESSION['susername'];

if(!isset($_POST['submit']) && !isset($_POST['cancel']))
{
	//checking for admin rites
	$handle = 	sqlite3_open($db) or die("Could not open database");
	$query = "SELECT * FROM users where username=\"$username\"";
	$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));
	$row = sqlite3_fetch_array($result);
	if ($row["admin"] == "false") { 
	echo "<div align=\"center\">You donot have a rite to approve users</div>";
	exit();
	}
	
	//extracting user information
	$query = "SELECT * FROM users where username=\"$accountid\"";
	$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));
	if (($row = sqlite3_fetch_array($result)) == '') {  
	echo "<div align=\"center\">Not a valid account id</div>";
	exit();
	}
	
	$name = $row["name"];	
	$email = $row["email"];
	$website = $row["website"];
	$desc = $row["description"];
	
	if($row["approved"] == "true") {
		$query = "SELECT * FROM approval where approved=\"$accountid\"";
		$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));
		$row = sqlite3_fetch_array($result);
		echo "<div align=\"center\"> Already <b>$accountid</b> is approved by <b>".$row["approvedby"]."</b>...</div>";
		exit();
	}
?>
	<h3 class="firstheader" align="center">Approval</h3>
	<form method="post" action="approval.php?accountid=<?=$accountid?>" >
	<table align="center" border="0" cellpadding="10" bgcolor="gray">
	 		<tr align="left" bgcolor="lightgray">
	 		<td> 	<p>Full name: </td><td><?=$name;?></p></td>
	</tr>
	<tr align="left" bgcolor="silver">
	 		<td> 	<p>Account id: </td><td><?=$accountid;?></p></td>
	</tr>
	 		<tr align="left" bgcolor="lightgray">
	 		<td> <p>Email: </td><td><?=$email;?></p>
	</td>
	</tr>
	 		<tr align="left" bgcolor="silver">
	 		<td> <p>Description: </td><td><?=$desc;?></p> </td>
	</tr>
	 		<tr align="left" bgcolor="lightgray">
	 		<td> <p>Web Site: </td><td><?=$website;?></p> </td>
	</tr>
	<tr align="left" bgcolor="lightgray">
	 		<td>Admin rites</td><td><input type="checkbox" name="isadmin"> (check this if you want this user to be admin) </td>
	</tr>
	 		<tr align="center">
	 		<td> <input type="submit" name="submit" value="Approve"/></td>
	 		<td><input type="submit" name="cancel" value="Cancel Approval"/></td>
	</tr>
	</table>
	</form><hr>
	</body>
	</html>
<?php
}

//if approved 
else if (isset($_POST['submit'])) {
//set approval=true
$handle = sqlite3_open($db) or die("Could not open database");
$query = "update users set approved=\"true\" where username=\"$accountid\"";
$result = sqlite3_exec($handle, $query) or die("Error in query: ".sqlite3_error($handle));

//where whether user is given admin permission 
if($_POST['isadmin']) {
$query = "update users set admin=\"true\" where username=\"$accountid\"";
$result = sqlite3_exec($handle, $query) or die("Error in query: ".sqlite3_error($handle));
}

//inserting to db 
$query = "insert into approval values(\"$accountid\",\"$username\")";
$result = sqlite3_exec($handle, $query) or die("Error in query: ".sqlite3_error($handle));

//get email id
$query = "SELECT * FROM users where username=\"$accountid\"";
$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));
$row = sqlite3_fetch_array($result);
$email = $row["email"];

//generate password and send mail
$password = generate_passwd(8);
$subject = "Approval of ccan account";
$message = "Your request for ccan account id is being approved.\n\n Please use the following password to login\n Password: ".$password;
$password = md5($password);

//insert password 
$query = "insert into login (username,password) values(\"$accountid\",\"$password\")";
$result = sqlite3_exec($handle, $query) or die("Error in query: ".sqlite3_error($handle));

//sendmail 
mail($email, $subject, $message, "From: $frommail");
echo "<div align=center> Successfully approved <b>$accountid</b>...</div>";
}

//if approval is canceled
else if (isset($_POST['cancel'])) {
//delete user 
$handle = sqlite3_open($db) or die("Could not open database");
$query = "delete from users where username=\"$accountid\"";
$result = sqlite3_exec($handle, $query) or die("Error in query: ".sqlite3_error($handle));
echo "<div align=center> Successfully cancelled <b>$accountid</b>...</div>";
}

function generate_passwd($length = 16) {
  static $chars = '!@#$%^&*abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNOPQRSTUVWXYZ23456789';
  $chars_len = strlen($chars);
  for ($i = 0; $i < $length; $i++)
    $password .= $chars[mt_rand(0, $chars_len - 1)];
  return $password;
}
?>
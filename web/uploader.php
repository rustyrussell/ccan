<?php
session_start();
include('logo.html');
include('menulist.html');
include('configuration');
include('functions.php');
include('searchengine.php');

if ($_FILES["uploadedfile"]["error"] > 0) {
  echo "Error: " . $_FILES["uploadedfile"]["error"] . "<br />";
  exit();
}

//list of file types supported 
if($_FILES["uploadedfile"]["type"] == "application/x-gzip" 
    	|| $_FILES["uploadedfile"]["type"] == "application/x-tar" 
    		|| $_FILES["uploadedfile"]["type"] == "application/x-bzip"
    			|| $_FILES["uploadedfile"]["type"] == "application/zip") {
      	
	$folder = substr($_FILES["uploadedfile"]["name"], 0, strpos($_FILES["uploadedfile"]["name"],'.'));
	move_uploaded_file($_FILES["uploadedfile"]["tmp_name"],
		$tempfolder . $_FILES["uploadedfile"]["name"]);
	
	//extracting code
	if($_FILES["uploadedfile"]["type"] == "application/zip") {
		exec('unzip '.$tempfolder.$_FILES["uploadedfile"]["name"].' -d '.$tempfolder, $op, $status);
	}
	else {
		exec('tar -xf '.$tempfolder.$_FILES["uploadedfile"]["name"].' -C '.$tempfolder, $op, $status);
	}
	checkerror($status,"Error: cannot extract(tar error).");	

	//if user not logged in
	if($_SESSION["slogged"] == false) {
		//move to temp folder 
		if (file_exists($temprepo . $folder))
			rmdirr($temprepo.$folder);
  	   rename($tempfolder.$folder, $temprepo.$folder);
		
		//send mail for review to admins 
		$subject = "Review: code upload at temporary repository"; 
		$message = "Some developer has uploaded code who has not logged in.\n\nModule is stored in ".$temprepo.$folder.".\n\nOutput of ccanlint: \n";
			
    	$toaddress = getccanadmin($db);
   	mail($toaddress, $subject, $message, "From: $frommail");
   	echo "<div align=\"center\"> Stored to temporary repository. Mail will be send to admin to get verification of the code.<//div>";
   	unlink($tempfolder.$_FILES["uploadedfile"]["name"]);
   	exit();
	} 

	//running ccanlint
	exec($ccanlint.$tempfolder.$folder, $score, $status);
		
	//if not junk code 
	if($status == 0) {
		$rename = $folder;
		$exactpath = $repopath . $_SESSION['susername'] .'/';
		
		if (file_exists($exactpath)) {
			echo "<div align=\"center\"> Your another upload is in progress please wait...</div>";
			exit();
		}
		
		//bzr local repo for commit
		chdir($repopath);
		unset($op); exec($bzr_clone . $_SESSION['susername'], $op, $status);
		checkerror($status, "Error: bzr local repo.");
		chdir('..');
				
		//if module already exist 
		if (file_exists($exactpath . $ccan_home_dir . $folder)) {

			// if owner is not same 
			if(!(getowner($ccan_home_dir . $folder, $db) == $_SESSION['susername'])) {	
				if(!file_exists($repopath . $ccan_home_dir . $folder . '-' . $_SESSION['susername']))   			
	   			echo "<div align=\"center\">". $ccan_home_dir . $folder . " already exists. Renaming to " . $folder . "-" . $_SESSION['susername'] . "</div>";
      		else
	   			echo "<div align=\"center\">". $ccan_home_dir . $folder . "-" . $_SESSION['susername'] . " already exists. Overwriting " . $folder. "-" . $_SESSION['susername'] . "</div>";
	   		$rename = $folder."-".$_SESSION['susername'];
	   	}
	   	
	   	else
	   		echo "<div align=\"center\">".$repopath. $ccan_home_dir. $folder. " already exists(uploaded by you). Overwriting ". $repopath. $folder."</div>";
	   			
  		}

  		//module not exist. store author to db 
  		else {
  			storefileowner($ccan_home_dir . $folder, $_SESSION['susername'], $db);
  		}

  		rmdirr($exactpath . $ccan_home_dir . $rename);
	   rename($tempfolder . $folder, $exactpath . $ccan_home_dir . $rename);
   		
   	chdir($exactpath);
		unset($op); exec($infotojson . $ccan_home_dir . $rename . " " . $ccan_home_dir. $rename."/_info.c ". $ccan_home_dir . $rename . "/json_" . $rename . " " . $_SESSION['susername']. " ../../" . $db, $op, $status);
		checkerror($status,"Error: In infotojson.");
		
		unset($op); exec('bzr add', $op, $status);
		checkerror($status,"Error: bzr add error.");
		
		unset($op); exec('bzr commit --unchanged -m "commiting from ccan web ' . $rename . " " . $_SESSION['susername'] . '"', $op, $status);
		checkerror($status,"Error: bzr commit error.");	
			
		unset($op); exec($bzr_push, $op, $status);
		checkerror($status,"Error: bzr push error.");
		
		chdir('../..');
		rmdirr($exactpath);
   	echo "<div align=\"center\"> Stored to ". $ccan_home_dir . $rename . "</div>";
	}
	
	//if junk code (no _info.c etc)	
	else {
	
		rmdirr($junkcode.$folder.'-'.$_SESSION['susername']);
		rename($tempfolder.$folder, $junkcode.$folder.'-'.$_SESSION['susername']);
		
		if($score == '')
			$msg =  'Below is details for test.';
			
		echo "<div align=\"center\"><table><tr><td> Score for code is low. Cannot copy to repository. Moving to ". $junkcode.$folder.'-'.$_SESSION['susername']."... </br></br>". $msg ." </br></br></td></tr><tr><td>";

		foreach($score as $disp)
			echo "$disp</br>";
		echo "</td></tr></table></div>";
		
	}
  	unlink($tempfolder.$_FILES["uploadedfile"]["name"]);
}
else {
	echo "<div align=\"center\"> File type not supported </div>";
	exit();
} 

function checkerror($status, $msg)
{
	if($status != 0) {
		    echo "<div align=\"center\">" . $msg . "Contact ccan admin. </div>";
		    exit();
	}
}

function getowner($filename, $db)
{
   //getting owner of a file stored at db 
	$handle = sqlite3_open($db) or die("Could not open database");
	$query = "SELECT owner FROM fileowner users where filename=\"$filename\"";
	$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));
	$row = sqlite3_fetch_array($result);
	return $row["owner"];
}

function storefileowner($filename, $owner, $db)
{
   //storing owner of a file stored at db 
	$handle = sqlite3_open($db) or die("Could not open database");
	$query = "insert into fileowner values(\"$filename\", \"$owner\")";
	$result = sqlite3_exec($handle, $query) or die("Error in query: ".sqlite3_error($handle));
}
?>
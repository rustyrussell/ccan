<?php
function createsearchindex($module, $path, $infofile, $db, $user) 
{
	$fh = fopen($path.$infofile, 'r') or die("Can't open file");

	$title = extract_title($fh);
	$desc = extract_desc($fh);
	//foreach($desc as $temp)
	//		$alldesc = $alldesc.$temp.'\n';
	$author = $user;
		
	//storing in search db 
	$handle = sqlite3_open($db) or die("Could not open database");
	$query = "select * from search where module=\"$module\"";	
	$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));
	
	if (($row = sqlite3_fetch_array($result)) == '') { 
		$query = "insert into search values(\"$module\",\"$user\",\"$title\",\"$alldesc\");";
	}
	else {
		$query = "update search set title=\"$title\", desc=\"$alldesc\" where module=\"$module\";";
	}
	$result = sqlite3_exec($handle, $query) or die("Error in query: ".sqlite3_error($handle));
	fclose($fh);
}

function extract_title($fh)
{
	while(substr(fgets($fh), 0, 3) != '/**');
	
	return substr(strstr(fgets($fh),'*'),1);
}

function extract_desc($fh)
{
$i = 0;
	while(substr(($line = fgets($fh)), 0, 2) == ' *') {
		$desc[$i] = substr(strstr($line,'*'),1);;
		$i = $i + 1;
			}
	return $desc;
}
?>

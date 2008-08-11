<?php
function rmdirr($dirname)
{
    // Sanity check
    if (!file_exists($dirname)) {
        return false;
    }
 
    // Simple delete for a file
    if (is_file($dirname) || is_link($dirname)) {
        return unlink($dirname);
    }
 
    // Loop through the folder
    $dir = dir($dirname);
    while (false !== $entry = $dir->read()) {
        // Skip pointers
        if ($entry == '.' || $entry == '..') {
            continue;
        }
 
        // Recurse
        rmdirr($dirname . DIRECTORY_SEPARATOR . $entry);
    }
 
    // Clean up
    $dir->close();
    return rmdir($dirname);
}

function getccanadmin($db)
{
	//forming admin mail addresses from data base	
	$handle = sqlite3_open($db) or die("Could not open database");
	$query = "SELECT email FROM users where admin=\"true\"";
	$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));

	while ($row = sqlite3_fetch_array($result))
   	 $admin = $admin.$row[0].",";
   return $admin; 
}
?>
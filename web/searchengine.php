<?php
function searchdb($text, $in, $db)
{
	//search db 
	$handle = sqlite3_open($db) or die("Could not open database");
	if($in == 'module')
		$query = "select * from search where title LIKE \"%$text%\"";
	else if($in == 'author')
		$query = "select * from search where author LIKE \"%$text%\"";
	else 	
		$query = "select * from search where title LIKE \"%$text%\" or author LIKE \"%$text%\"";
		
	$result = sqlite3_query($handle, $query) or die("Error in query: ".sqlite3_error($handle));
	return $result;
}

?>
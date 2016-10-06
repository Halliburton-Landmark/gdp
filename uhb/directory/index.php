<!DOCTYPE HTML>  
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1">
<style>
.error {color: #FF0000;}
</style>
</head>
<body>  

<?php
// define variables and set to empty values

$hash = "$2y$10$/KaZ.U5S4oJCYvuRsm9dMepZu.5mce85EQGF7O5YZpsXlYHy5.CRi";
//$hash = password_hash("welcome", PASSWORD_DEFAULT);

$passwordErr = $keyErr = $lognameErr = $humannameErr = $locationErr = $sensortypeErr = "";
$conn = $password = $keys = $logname = $humanname = $sensortype = $location= "";
$passmatch = false;
//phpinfo();

$servername = "localhost";
$username = "gdp";
$pass = "swarmlab";
$dbname = "sensor_db";
// Create connection
$conn = new mysqli($servername, $username, $pass, $dbname);

// Check connection
if ($conn->connect_error) {
die("Connection failed: " . $conn->connect_error);
} 
echo "Connected successfully\n";

function close_sql() {
    $conn->close();
} 
function insert_sql($humanname, $location, $logname, $sensortype, $keys) {
    echo $humanname;
}
//function select_type() {
$sql = "SELECT type_name FROM type";
$types = $conn->query($sql);
/*
if ($types->num_rows > 0) {
// output data of each row
    while($row = $types->fetch_assoc()) {
        echo $row["type_name"];
    }
} else {
    echo "0 results";
}
*/

function select_keys($type) {
    if ($sql = $conn->prepare("SELECT metric_name, value_type FROM metric as m join type as t on m.t_id = t.type_id where t.type_name = ?")) {
        $sql->bind_param("s", $type);
        $sql->execute();
        $sql->bind_result($result);
        $sql->fetch();
        
   // $result = $conn->query($sql);

    if ($result->num_rows > 0) {
        // output data of each row
        while($row = $result->fetch_assoc()) {
        echo $row["type_name"];
        }
    } else {
        echo "0 results";
    } 
    $sql->close();
    }
}

function parse_keys($metrics) {
//should be: 'metric name : value type , ...'
    $final = [];
    $result = array_filter(explode("," , $metrics));
    foreach ($result as $pair) {
        $inner = array_filter(explode(":" , $pair));
        if (count($inner) == 2) {
           $final[trim($inner[0])] = trim($inner[1]);
        }
    }
foreach ($final as $x => $x_value) {
    echo $x . " : " . $x_value;
    echo ", ";
}
    return $final;
}


if ($_SERVER["REQUEST_METHOD"] == "POST") { 
  if (empty($_POST["humanname"])) {
    $humannameErr = "Human name is required";
  } else {
    $humanname = test_input($_POST["humanname"]);
    // check if name is well-formed
    if (!preg_match("/^[a-zA-Z ]*$/",$humanname)) {
      $humannameErr = "Only letters and white space allowed";    
    }
  }
    
  if (empty($_POST["logname"])) {
    $lognameErr = "Log name is required";
  } else {
    $logname = test_input($_POST["logname"]);
    // check if logname only contains letters and whitespace
    if (!preg_match("/^[a-zA-Z0-9_-~+.:<> ]*$/",$logname)) {
      $lognameErr = "Invalid characters given"; 
    }
  }

  if (empty($_POST["sensortype"])) {
    $sensortypeErr = "Sensor type is required";
  } else {
    $sensortype = test_input($_POST["sensortype"]);
    // check if syntax is valid (this regular expression also allows dashes in the URL)
    if (!preg_match("/^[a-zA-Z0-9-*_~.,' ]*$/",$sensortype)) {
      $sensortypeErr = "Invalid characters given"; 
    }
  }

  if (empty($_POST["location"])) {
    $location = "";
  } else {
    $location = test_input($_POST["location"]);
    // check if syntax is valid (this regular expression also allows dashes in the URL)
    if (!preg_match("/^[a-zA-Z0-9-.,;' ]*$/",$location)) {
      $locationErr = "Invalid characters given"; 
    }
  }

 if (empty($_POST["keys"])) {
    $keys = "";
  } else {
    $keys = test_input($_POST["keys"]);
    // check if syntax is valid
    if (!preg_match("/^[a-zA-Z0-9-_.,:;~ ]*$/",$keys)) {
      $keyErr = "Invalid characters given"; 
    }
    if (!is_array($keys)) {
        $keys = parse_keys($keys);
    } 
  }

 if (empty($_POST["password"])) {
    $passwordErr = "Password is required to submit form";
  } else {
    $password = test_input($_POST["password"]);
    // check if URL address syntax is valid (this regular expression also allows dashes in the URL)
    if (!password_verify($password, $hash)) {
    $passwordErr = "Password doesn't match";
    $passMatch = false;
    } else {
      $passwordErr = "Match!";
      $passMatch = true;
    }
  }
}
function test_input($data) {
  $data = trim($data);
  $data = stripslashes($data);
  $data = htmlspecialchars($data);
  return $data;
}
?>
<style>
select { font-size: 14px;
}
input { font-size: 14px;
 }
textarea { font-size: 13px; }
label textarea{
 vertical-align: top;
}
</style>
<h2>PHP Form Validation Example</h2>
<p><span class="error">* required field.</span></p>
<form method="post" action="<?php 
echo htmlspecialchars($_SERVER["PHP_SELF"]);?>">  
  Human's name (first and last): <input type="text" name="humanname" value="<?php echo $humanname;?>">
  <span class="error">* <?php echo $humannameErr;?></span>
  <br><br>
  Location: <input style="width: 25%;" type="text" name="location" value="<?php echo $location;?>">
<br><label for="location"><small>Example: 'University name, building name'</small></label>  <span class="error"><?php echo $locationErr;?></span>
<br><br>
  Logname: <input style="width: 25%;" type="text" name="logname" value="<?php echo $logname;?>">
  <span class="error">* <?php echo $lognameErr;?></span>
  <br><br>
  Sensor Type: <input list="types" name="sensortype" value="<?php echo $sensortype;?>">
    <datalist id="types">
    <?php 
    while ($row = $types->fetch_assoc()){
        echo "<option value=" . $row['type_name'] . ">" . $row['type_name'] . "</option>";
    } 
    ?></datalist>
  <span class="error">* <?php echo $sensortypeErr;?></span>
<br><br>
<label>  Sensor Metrics: <textarea list="types" name="keys" rows="5" cols="70">
<?php
$sql = "SELECT metric_name, value_type FROM metric JOIN type on metric.t_id = type.type_id WHERE type_name LIKE '".$sensortype."'";
//$q = "INSERT INTO `users`(`username`, `password`) VALUES ('".$username."', '".$password."')";
$result = $conn->query($sql);
if (!$result) {
    echo "Query failed: (" . $conn->errno . ") " . $conn->error;
} else {
     if (mysqli_num_rows($result) > 0) {
         $keys="";
	 while ($entry = $result->fetch_assoc()) {
	     $keys[$entry["metric_name"]] = $entry["value_type"];
         } 
     } else {
        //echo $keys; 
     }         
}
foreach ($keys as $x => $x_value) {
    echo $x . " : " . $x_value;
    echo ", ";
}
?></textarea></label>  <span class="error"><?php echo $keyErr;?></span>
<br><label for="keys"><small>Example: 'pressure_pascals : float, current_motion : bool'</small></label>  
  <br><br>

  Password: <input type="password" name="password">
  <span class="error">* <?php echo $passwordErr;?></span>
  <br><br>
<input type="submit" name="submit" value="Submit">   
</form>

<?php
echo "<h2>Your Input:</h2>";
echo $humanname;
echo "<br>";
echo $logname;
echo "<br>";
echo $sensortype;
echo "<br>";
echo $location;
echo "<br>";
foreach ($keys as $x => $x_value) {
    echo $x . " : " . $x_value;
    echo ", ";
}
echo "<br>";
echo $passMatch;
echo "<br>";
?>

</body>
</html>

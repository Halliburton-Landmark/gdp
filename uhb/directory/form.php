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
$lognameErr = $emailErr = $dataformatErr = $sensortypeErr = "";
$logname = $email = $sensortype = $dataformat= "";

if ($_SERVER["REQUEST_METHOD"] == "POST") { 
  if (empty($_POST["email"])) {
    $emailErr = "Email is required";
  } else {
    $email = test_input($_POST["email"]);
    // check if e-mail address is well-formed
    if (!filter_var($email, FILTER_VALIDATE_EMAIL)) {
      $emailErr = "Invalid email format"; 
    }
  }
    
  if (empty($_POST["logname"])) {
    $lognameErr = "Logname is required";
  } else {
    $logname = test_input($_POST["logname"]);
    // check if logname only contains letters and whitespace
    if (!preg_match("/^[a-zA-Z ]*$/",$logname)) {
      $lognameErr = "Only letters and white space allowed"; 
    }
  }

  if (empty($_POST["sensortype"])) {
    $sensortype = "";
  } else {
    $sensortype = test_input($_POST["sensortype"]);
    // check if URL address syntax is valid (this regular expression also allows dashes in the URL)
    if (!preg_match("/\b(?:(?:https?|ftp):\/\/|www\.)[-a-z0-9+&@#\/%?=~_|!:,.;]*[-a-z0-9+&@#\/%=~_|]/i",$sensortype)) {
      $sensortypeErr = "Invalid URL"; 
    }
  }

  if (empty($_POST["dataformat"])) {
    $dataformat = "";
  } else {
    $dataformat = test_input($_POST["dataformat"]);
    // check if URL address syntax is valid (this regular expression also allows dashes in the URL)
    if (!preg_match("/\b(?:(?:https?|ftp):\/\/|www\.)[-a-z0-9+&@#\/%?=~_|!:,.;]*[-a-z0-9+&@#\/%=~_|]/i",$dataformat)) {
      $dataformatErr = "Invalid URL"; 
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

<h2>PHP Form Validation Example</h2>
<p><span class="error">* required field.</span></p>
<form method="post" action="<?php 
echo htmlspecialchars($_SERVER["PHP_SELF"]);?>">  
  E-mail: <input type="text" name="email" value="<?php echo $email;?>">
  <span class="error">* <?php echo $emailErr;?></span>
  <br><br>
  Logname: <input type="text" name="logname" value="<?php echo $logname;?>">
  <span class="error">* <?php echo $lognameErr;?></span>
  <br><br>
  Sensor Type: <input type="text" name="sensortype" value="<?php echo $sensortype;?>">
  <span class="error"><?php echo $sensortypeErr;?></span>
  <br><br>
  Data Format (JSON,XML): <input type="text" name="dataformat" value="<?php echo $dataformat;?>">
  <span class="error"><?php echo $dataformatErr;?></span>
  <br><br> <input type="submit" name="submit" value="Submit">  
</form>

<?php
echo "<h2>Your Input:</h2>";
echo $email;
echo "<br>";
echo $logname;
echo "<br>";
echo $sensortype;
echo "<br>";
echo $dataformat;
echo "<br>";
?>

</body>
</html>

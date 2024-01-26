<?php

function generateCgiCommand($cgiScript, $parameters = []) {
    // Construct the base command with the CGI script
    $command = "$cgiScript";

    // Add input parameters to the command
    if (!empty($parameters)) {
        $command .= '?' . http_build_query($parameters);
    }

    return $command;
}

// Specify the path to your CGI script
echrggo rgrgrrgr;
$cgiScript = '/usr/bin/php-cgi';

// Example input parameters for the CGI script
$inputParameters = [
    'param1' => 'value1',
    'param2' => 'value2',
    'param3' => 'value3'
];

// Generate the CGI command
$command = generateCgiCommand($cgiScript, $inputParameters);

// Execute the command and capture the output
exec($command, $output, $returnCode);

// Check the return code to see if the execution was successful
if ($returnCode === 0) {
    echo "CGI script executed successfully.\n";
    echo "Output:\n";
    echo implode("\n", $output);
} else {
    echo "Error executing CGI script. Return code: $returnCode\n";
}

?>

<?php 
	// execute:
	// cd /cs/systems/home/sba70/octopus_garden/clavis-src/
	// emerge -1 libpng:1.2
	// php global-workload-snapshot.php

	session_start(); 

	ini_set("max_execution_time", "600");
	// need to change also in php.ini (issue locate php.ini in shell), as the smallest limit found wherever is the active one
	ini_set('memory_limit', '-1');
	date_default_timezone_set('America/Los_Angeles');
	
	$RANK_PER_APP = 12;
	
	$log_file_rel = "/cs/systems/home/sba70/octopus_garden/clavis-src/comm-matrix-rel-ids-$RANK_PER_APP.csv";
	$log_file_abs = "/cs/systems/home/sba70/octopus_garden/clavis-src/comm-matrix-abs-ids-$RANK_PER_APP.csv";
	$choco_log_file = "/cs/systems/home/sba70/octopus_garden/clavis-src/comm-matrix-for-choco-$RANK_PER_APP.csv";
	$matlab_log_file = "/cs/systems/home/sba70/octopus_garden/clavis-src/comm-matrix-for-matlab-$RANK_PER_APP.csv";

	$choco_classes_log_file = "/cs/systems/home/sba70/octopus_garden/clavis-src/classes-for-choco-$RANK_PER_APP.csv";
	$matlab_classes_log_file = "/cs/systems/home/sba70/octopus_garden/clavis-src/classes-for-matlab-$RANK_PER_APP.csv";
	
	function append_line_to_log($filename, $log_line)
	{
		$fh = fopen($filename, 'a') or die("can't open file $filename");
		fwrite($fh, $log_line);
		fclose($fh);
	}
	
	
	//$hostname=`hostname`;
	//$today = date("D M j G:i:s T Y");

  
  
  
  
  /*
  $dir    = '/mnt/kvm_images/sergey_vms/upload/clavis-reports/';
  $reports = scandir($dir);
  
  for ($report_id=2; $report_id<count($reports); $report_id++)
  {
        $server_name = $reports[$report_id];
        $server_name = str_replace(".report", "", $server_name);
        
        
        // read the Clavis trace 
        $vector_filename = $dir . $reports[$report_id];

      	$k = 0;
      	$pieces = array();
      	$latest_record = array();
      	$latest_complete_record = array();
      	$cur_time = -1;
      	$array_n = 0;
      	$timestamp_change = 0;
      	
      	$global_vm_names[$server_name] = array();
      	
      	$totalcpu = 0;
      	
      	if (file_exists($vector_filename)) {
            //// reading backwards for the latest info, show records for the timestamp which is next after last one (those are all there for sure, the records for the latest timestamp might still be generated)
            //$file = popen("tac $vector_filename",'r');
            
            $file = fopen($vector_filename,'r') or die($php_errormsg);

            while ($cur_vector_line = fgets($file)) {
      				$cur_vector_line = str_replace("    ", " ", $cur_vector_line);
      				$cur_vector_line = str_replace("   ", " ", $cur_vector_line);
      				$cur_vector_line = str_replace("  ", " ", $cur_vector_line);
      				
      				$pieces = explode(" ", $cur_vector_line);
      				
      				$cur_time = $pieces[0];
      				if ($cur_time <= 0) continue;
      				
      				// gathering cpu utilization trace
      				$record_name = $pieces[7];
        			$record_time = $pieces[1]." ".$pieces[2]." ".$pieces[3]." ".$pieces[4]." ".$pieces[5];
        			$record_time = substr($record_time, 1, count($record_time)-3);
        			$record_time_timestamp = strtotime($record_time);
        			$record_cpu = $pieces[9];
        			if ($record_time_timestamp >= $experiment_start_timestamp && $record_time_timestamp <= $experiment_end_timestamp)
        			{
        			    if (in_array($record_name, $global_vm_names[$server_name]) == false) {
                      array_push($global_vm_names[$server_name], $record_name);
                      
                      // so that x-axis is the same on all graphs
                      $_SESSION['GISTO_X'][$record_name."-".$server_name][0] = $experiment_start_timestamp; //date('j M Y h:i:s A', $experiment_start_timestamp);
                      $_SESSION['GISTO_Y'][$record_name."-".$server_name][0] = 0;
        			    }
        			    
        			    // fill in each second in between measurements with the previous value
        			    $i = count($_SESSION['GISTO_X'][$record_name."-".$server_name]);
        			    $timestamp_i = $_SESSION['GISTO_X'][$record_name."-".$server_name][$i-1] + 1;
        			    $value_i = 0;
        			    if (   ($record_time_timestamp - $timestamp_i) < $MIGRATION_BREAK_IN_SECONDS   )
        			    {
            			    $value_i = $_SESSION['GISTO_Y'][$record_name."-".$server_name][$i-1];
        			    }
        			    while ($timestamp_i < $record_time_timestamp)
        			    {
                      $_SESSION['GISTO_X'][$record_name."-".$server_name][$i] = $timestamp_i;
                      $_SESSION['GISTO_Y'][$record_name."-".$server_name][$i] = $value_i;
                      $timestamp_i+=$SAMPLE_INTERVAL_IN_SECONDS;
                      $i++;
                  }

        			    
                  $_SESSION['GISTO_X'][$record_name."-".$server_name][$i] = $record_time_timestamp; //date('h:i:s', $record_time_timestamp);
                  $_SESSION['GISTO_Y'][$record_name."-".$server_name][$i] = $record_cpu;

          				if ($cur_time != $latest_record["array_time"][0])
          				{
                      // timestamp change, saving the latest complete record (the very last in the log file might be incomplete)
                      $latest_complete_record = $latest_record;
    
                      $latest_record = array();
                      $latest_record["array_time"][0] = $cur_time;
            					$array_n = 0;


            			    if ($totalcpu == 0) {
                          $totalcpu = 1;
                          
                          // so that x-axis is the same on all graphs
                          $_SESSION['GISTO_X']["totalcpu"."-".$server_name][0] = $experiment_start_timestamp; //date('j M Y h:i:s A', $experiment_start_timestamp);
                          $_SESSION['GISTO_Y']["totalcpu"."-".$server_name][0] = 0;
            			    }
            			    
            			    if (count($latest_complete_record["names"]) > 0)
            			    {
                			    $total_record_time_timestamp = strtotime($latest_complete_record["times"][0]);
    
                			    // fill in each second in between measurements with the previous value
                			    $i = count($_SESSION['GISTO_X']["totalcpu"."-".$server_name]);
                			    $timestamp_i = $_SESSION['GISTO_X']["totalcpu"."-".$server_name][$i-1] + 1;
                			    $value_i = 0;
                			    if (   ($total_record_time_timestamp - $timestamp_i) < $MIGRATION_BREAK_IN_SECONDS   )
                			    {
                    			    $value_i = $_SESSION['GISTO_Y']["totalcpu"."-".$server_name][$i-1];
                          }
                			    while ($timestamp_i < $total_record_time_timestamp)
                			    {
                              $_SESSION['GISTO_X']["totalcpu"."-".$server_name][$i] = $timestamp_i;
                              $_SESSION['GISTO_Y']["totalcpu"."-".$server_name][$i] = $value_i;
                              $timestamp_i+=$SAMPLE_INTERVAL_IN_SECONDS;
                              $i++;
                          }
                			    
                			    
                          $_SESSION['GISTO_X']["totalcpu"."-".$server_name][$i] = $total_record_time_timestamp; //date('h:i:s', $total_record_time_timestamp);
                          $_SESSION['GISTO_Y']["totalcpu"."-".$server_name][$i] = 0;
                        	for ($total_k=0; $total_k<count($latest_complete_record["names"]); $total_k++) {
                              $_SESSION['GISTO_Y']["totalcpu"."-".$server_name][$i] += $latest_complete_record["cpu_utilization"][$total_k];
                        	}
                        	if ($_SESSION['GISTO_Y']["totalcpu"."-".$server_name][$i] > $numberOfProcessors * 100)
                              $_SESSION['GISTO_Y']["totalcpu"."-".$server_name][$i] = $numberOfProcessors * 100;
                      }
          				}
          				
            			$latest_record["names"][$array_n] = $record_name;
            			$latest_record["pids"][$array_n] = $pieces[6];
            			$latest_record["times"][$array_n] = $record_time;
            			$latest_record["cpu_utilization"][$array_n] = $record_cpu;
            			$latest_record["memory_usage"][$array_n] = $pieces[14];
            			$latest_record["traffic_sent"][$array_n] = $pieces[17];
            			$latest_record["traffic_received"][$array_n] = $pieces[19];
            			$latest_record["io_write"][$array_n] = $pieces[22];
            			$latest_record["io_read"][$array_n] = $pieces[24];
            			$array_n++;
              }
      				
      				
      				// 8***0 (Mon***1 Jan***2  2***3 01:10:36***4 2012):***5 26450***6 nze007***7   CPU***8 136***9 MISS***10 RATE***11 979.285202***12   MEM***13 12.700000***14   TRAFFIC***15 SNT***16 0.000000***17 RCVD***18 0.000000***19   IO***20 WRITE***21 242.460000***22 READ***23 0.000000***24
            }
      
      			pclose($file);
      	}
      	else
      	{
      		echo "File $vector_filename does not exist!";
      	}
	}
    */




	
	// read the tau traces for communication matrix
	$tau_traces_root_dir    = '/cs/systems/home/sba70/octopus_garden/clusters/MPI2007-rundirs/';
	$appdirs = scandir($tau_traces_root_dir);
	$abs_max_rank_before_cur_app = 0;
	
	$comm_matrix = array();
	$abs_ranks_before = array(); // needed when generating a single comm matrix array in Choco and Matlab
	for ($appdir_id=2; $appdir_id<count($appdirs); $appdir_id++)
	{
	    $app_name = $appdirs[$appdir_id];

		$tau_traces_cur_dir    = $tau_traces_root_dir . $app_name . '/tau'.$RANK_PER_APP.'/';
      	if (file_exists($tau_traces_cur_dir)) {
			$comm_matrix[$app_name] = array();
			$abs_ranks_before[$app_name] = $abs_max_rank_before_cur_app;
			$abs_max_rank_before_cur_app += $RANK_PER_APP;
			
			$curfiles = scandir($tau_traces_cur_dir);
			for ($curfile_id=2; $curfile_id<count($curfiles); $curfile_id++)
			{
				$curfile = $curfiles[$curfile_id];
				if (substr($curfile, 0, 7) == "profile")
				{
					$temp = explode('.', $curfile);
					$rank = $temp[1];
					
					$file = fopen($tau_traces_cur_dir . $curfile,'r') or die($php_errormsg);
					while ($cur_line = fgets($file)) {
						if (substr($cur_line, 0, 27) == '"Message size sent to node ')
						{
							$cur_line = substr($cur_line, 27, 10);
							$temp = explode('"', $cur_line);
							if (sizeof($temp) > 1)
							{
								$rank_to = $temp[0];
								
								$comm_matrix[$app_name][$rank] .= "," . $rank_to;
							}
						}
					}
					
					pclose($file);
				}
			}
		}
	}
	
	
	
	
	//========================================================================================
	//================================ generating logs =======================================
	//========================================================================================
	  
    $fh = fopen($log_file_rel, 'w');
	fclose($fh);
    $fh = fopen($log_file_abs, 'w');
	fclose($fh);
    $fh = fopen($choco_log_file, 'w');
	fclose($fh);
    $fh = fopen($matlab_log_file, 'w');
	fclose($fh);
    $fh = fopen($choco_classes_log_file, 'w');
	fclose($fh);
    $fh = fopen($matlab_classes_log_file, 'w');
	fclose($fh);

	$choco_classes_line = "";
	$matlab_classes_line = "";

	$matlab_line_1 = "";
	$matlab_line_2 = "";
	append_line_to_log($log_file_rel, "app_name,rel_rank_from,rel_ranks_to"."\n");
	append_line_to_log($log_file_abs, "app_name,abs_rank_from,abs_ranks_to"."\n");
	foreach ($comm_matrix as $app_name => $ranks) {
		echo $app_name . " ";
		$class = 1;
		//if ($app_name == "pop" || $app_name == "zeusmp" || $app_name == "milc" || $app_name == "lu" || $app_name == "fds")
		if ($app_name == "pop" || $app_name == "zeusmp" || $app_name == "milc")
		//if ($app_name == "pop")
		{
			$class = 0;
		}

		for ($j=0; $j<$RANK_PER_APP; $j++)
		{
			$choco_classes_line .= "$class,";
			$matlab_classes_line .= "$class,";
		}
		
		foreach ($ranks as $rank => $ranks_to) {
			append_line_to_log($log_file_rel, $app_name.",".$rank.$ranks_to."\n");
			
			$abs_rank = $rank+$abs_ranks_before[$app_name];
			$choco_line = $abs_rank;
			
			$temp = explode(',', $ranks_to);
			for ($i=1; $i<count($temp); $i++)
			{
				$choco_line .= ",".($temp[$i]+$abs_ranks_before[$app_name]);
				
				$matlab_line_1 .= ($abs_rank+1). ",";
				$matlab_line_2 .= ($temp[$i]+$abs_ranks_before[$app_name]+1) . ",";
			}
			
			append_line_to_log($choco_log_file,$choco_line."\n");
			append_line_to_log($log_file_abs, $app_name.",".$choco_line."\n");
		}
	}
	$matlab_line_1 = substr($matlab_line_1,0,-1);
	$matlab_line_2 = substr($matlab_line_2,0,-1);
	append_line_to_log($matlab_log_file, $matlab_line_1."\n");
	append_line_to_log($matlab_log_file, $matlab_line_2."\n");
	
	$choco_classes_line = substr($choco_classes_line,0,-1);
	$matlab_classes_line = substr($matlab_classes_line,0,-1);
	append_line_to_log($choco_classes_log_file, $choco_classes_line."\n");
	append_line_to_log($matlab_classes_log_file, $matlab_classes_line."\n");
?>

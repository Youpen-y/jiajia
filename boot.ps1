$command = "docker run -itd --privileged=true --name JIAJIA " +
           "--mount type=bind,source=""D:\loongprojects\jiajia"",target=""/root/JIAJIA"" " +
           "origin:v3 bash"

Invoke-Expression $command
param(
	[string]$port = "7070",
	[string]$path = "./",
	[string]$depth = "1"
)
$compareFile = "compare_res"
$outDir = "out"
$excludes = 'test[\\/]'
Write-Output "Testing files in $path
Recursion limited to $depth
Port $port"
if ($(mycurl gopher://localhost:$port// 2>&1) -match '[Ff]ail') {
	Write-Output "Failed to connect to server"
	exit
}
if (Test-Path $compareFile) {
	Clear-Content $compareFile
}
else {
	New-Item $compareFile *>$null
}
if (!(Test-Path $outDir)) {
	mkdir $outDir *>$null
}
if (Test-Path $outDir) {
	Remove-Item -r "$outDir/*"
}
$result = $(Get-ChildItem -Recurse -Depth $depth -Path $path | 
	Where-Object { $_.FullName -notmatch ($excludes) } | 
	ForEach-Object {
		$relPath = ($_ | Resolve-Path -Relative).replace($path, "")
		$isDir = $(Test-Path -Path $_.fullname -PathType Container)
		$out = $relPath -replace '[\\/]', '_'
		mycurl gopher://localhost:$port//$relPath --output $outDir/$out *>$null 
		if (!$isDir) {
			$comp = (fc.exe /b $path$relPath $outDir\$out 2>&1) | Out-String
			Write-Output $comp | Out-File $compareFile -Append
			return $comp -notmatch '[A-Z\d]{8}: ([A-Z\d]{2} ?){2}'
		}
	}) -notcontains $False
if ($result) {
	Write-Output "All tests successfull!"
}
else {
	Write-Output "Some tests failed"
}
Write-Output "Check comparison results in $compareFile"
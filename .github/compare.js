const child_process = require('child_process');

let command = 'magick compare';

if (process.platform === 'linux') {
	command = 'compare-im6';
}
else if (process.platform === 'darwin') {
	command = 'export DYLD_LIBRARY_PATH="$HOME/imagemagick/lib/" && $HOME/imagemagick/bin/magick compare';
}

const proc = child_process.spawnSync(command, ['-metric', 'mae', './reference.png', './test.png', 'difference.png'], {encoding: 'utf8'});
if (proc.status !== 0) {
	const compare = parseFloat(proc.stderr.substring(proc.stderr.indexOf('(') + 1, proc.stderr.indexOf(')')));
	console.log('Compare value is ' + compare + '.');
	if (compare > 0.001) {
		console.log('That\'s not good enough.');
		process.exit(1);
	}
}

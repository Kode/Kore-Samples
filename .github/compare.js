const child_process = require('child_process');

const proc = child_process.spawnSync('magick', ['compare', '-metric', 'mae', './reference.png', './test.png', 'difference.png'], {encoding: 'utf8'});
if (proc.status !== 0) {
	const compare = parseFloat(proc.stderr.substring(proc.stderr.indexOf('(') + 1, proc.stderr.indexOf(')')));
	console.log('Compare value is ' + compare + '.');
	if (compare > 0.001) {
		console.log('That\'s not good enough.');
		process.exit(1);
	}
}

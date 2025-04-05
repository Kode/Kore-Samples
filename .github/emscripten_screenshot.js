const puppeteer = require("puppeteer");
const nstatic = require('node-static');
const core = require('@actions/core');

const fs = require('fs');
const path = require('path');

core.info('Running server on 8888');
const fileServer = new nstatic.Server(path.join(process.cwd(), 'build', 'debug'), { cache: 0 });

const server = require('http').createServer((request, response) => {
	request.addListener('end', function () {
		fileServer.serve(request, response);
	}).resume();
});

server.on('error', (e) => {
	if (e.code === 'EADDRINUSE') {
		core.info('Error: Port 8888 is already in use.');
	}
});

server.listen(8888);

core.info('Starting browser');
(async () => {
	const browser = await puppeteer.launch({args: ['--no-sandbox']});
	const page = await browser.newPage();

	page.on('console', msg => core.info('Page: ' + msg.text));

	await page.goto('http://localhost:8888');

	const client = await page.target().createCDPSession();
	await client.send('Page.setDownloadBehavior', {
		behavior: 'allow',
		downloadPath: path.join(process.cwd(), 'deployment'),
	});

	setTimeout(async () => {
		await browser.close();
		server.close();
		core.info('Browser and server closed');
	}, 10000);
})();

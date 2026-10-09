// Public API reads only: node .github/ci/dry-run.cjs [commit SHA]
// Runs the production decision helper with a small fetch-backed Octokit adapter.
const decide = require('./decide.cjs');
const sha = process.argv[2] || '2ecacc4f16bc44d468998665b0762487f7e69233';
const routes = {
  getCommit: p => `/repos/${p.owner}/${p.repo}/commits/${p.ref}`,
  listWorkflowRuns: p => `/repos/${p.owner}/${p.repo}/actions/workflows/${p.workflow_id}/runs`,
  listWorkflowRunArtifacts: p => `/repos/${p.owner}/${p.repo}/actions/runs/${p.run_id}/artifacts`
};
const method = name => Object.assign(async p => {
  const url = new URL(`https://api.github.com${routes[name](p)}`);
  for (const key of ['event', 'status', 'head_sha', 'per_page', 'page']) {
    if (p[key] !== undefined) url.searchParams.set(key, p[key]);
  }
  console.log(`GET ${url}`);
  const response = await fetch(url, { headers: { Accept: 'application/vnd.github+json' } });
  if (!response.ok) throw new Error(`${response.status}: ${await response.text()}`);
  const data = await response.json();
  if (name === 'listWorkflowRunArtifacts') {
    console.log(`Artifacts: ${data.artifacts.map(a => `${a.name} (expired=${a.expired})`).join(', ')}`);
  }
  return { data };
}, { endpointName: name });
const github = {
  rest: { repos: { getCommit: method('getCommit') }, actions: {
    listWorkflowRuns: method('listWorkflowRuns'),
    listWorkflowRunArtifacts: method('listWorkflowRunArtifacts')
  } },
  paginate: async (fn, params) => {
    const all = [];
    for (let page = 1; ; page++) {
      const { data } = await fn({ ...params, page });
      const items = data.workflow_runs || data.artifacts;
      all.push(...items);
      if (items.length < params.per_page) return all;
    }
  }
};
(async () => {
  for (const [workflow, packages] of [
    ['linux.yml', ['opentyrian-linux-x86_64', 'opentyrian-linux-arm64']],
    ['macos.yml', ['opentyrian-macos-universal']],
    ['windows.yml', ['opentyrian-windows-x86_64']]
  ]) {
    for (const eventName of ['push', 'release']) {
      console.log(`\n${workflow}: ${eventName}`);
      await decide({ github, workflow, packages,
        context: { repo: { owner: 'vittau', repo: 'modern-tyrian' }, sha, runId: -1,
          eventName, payload: { release: { tag_name: sha } } },
        core: { info: console.log, warning: console.warn,
          setOutput: (key, value) => console.log(`OUTPUT ${key}=${value}`),
          setFailed: message => { throw new Error(message); } }
      });
    }
  }
})().catch(error => { console.error(error); process.exitCode = 1; });

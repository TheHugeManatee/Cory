import { spawn, spawnSync } from "node:child_process";
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { fileURLToPath } from "node:url";

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { getAgentDir } from "@earendil-works/pi-coding-agent";
import { Text } from "@earendil-works/pi-tui";
import { Type } from "typebox";

type SizeConfig = {
	model: string;
	tools?: string[];
	systemPrompt?: string;
};

type RawConfig = {
	defaultSize?: string;
	piCommand?: string;
	stallTimeoutSeconds?: number;
	defaultTools?: string[];
	defaultSystemPrompt?: string;
	sizes?: Record<string, string | Partial<SizeConfig> & { id?: string }>;
	[size: string]: unknown;
};

type Config = {
	defaultSize?: string;
	piCommand?: string;
	stallTimeoutSeconds?: number;
	defaultTools?: string[];
	defaultSystemPrompt?: string;
	sizes: Record<string, SizeConfig>;
};

function normalizeSizeConfig(value: unknown): SizeConfig | undefined {
	if (typeof value === "string") {
		return { model: value };
	}

	if (!value || typeof value !== "object") {
		return undefined;
	}

	const record = value as Record<string, unknown>;
	const model = typeof record.model === "string" ? record.model : typeof record.id === "string" ? record.id : undefined;
	if (!model) return undefined;

	const tools = Array.isArray(record.tools)
		? record.tools.filter((tool): tool is string => typeof tool === "string" && tool.length > 0)
		: undefined;

	return {
		model,
		tools: tools && tools.length > 0 ? tools : undefined,
		systemPrompt: typeof record.systemPrompt === "string" ? record.systemPrompt : undefined,
	};
}

function normalizeConfig(raw: RawConfig): Config {
	const sizes: Record<string, SizeConfig> = {};
	const rawSizes = raw.sizes ?? raw;

	for (const [key, value] of Object.entries(rawSizes)) {
		if (key === "defaultSize" || key === "piCommand" || key === "defaultTools" || key === "defaultSystemPrompt") {
			continue;
		}

		const sizeConfig = normalizeSizeConfig(value);
		if (sizeConfig) sizes[key] = sizeConfig;
	}

	return {
		defaultSize: typeof raw.defaultSize === "string" ? raw.defaultSize : undefined,
		piCommand: typeof raw.piCommand === "string" ? raw.piCommand : undefined,
		stallTimeoutSeconds: typeof raw.stallTimeoutSeconds === "number" && Number.isFinite(raw.stallTimeoutSeconds) ? raw.stallTimeoutSeconds : undefined,
		defaultTools: Array.isArray(raw.defaultTools)
			? raw.defaultTools.filter((tool): tool is string => typeof tool === "string" && tool.length > 0)
			: undefined,
		defaultSystemPrompt: typeof raw.defaultSystemPrompt === "string" ? raw.defaultSystemPrompt : undefined,
		sizes,
	};
}

function mergeConfigs(base: Config | undefined, override: Config | undefined): Config {
	if (!base) return override ?? { sizes: {} };
	if (!override) return base;

	return {
		defaultSize: override.defaultSize ?? base.defaultSize,
		piCommand: override.piCommand ?? base.piCommand,
		stallTimeoutSeconds: override.stallTimeoutSeconds ?? base.stallTimeoutSeconds,
		defaultTools: override.defaultTools ?? base.defaultTools,
		defaultSystemPrompt: override.defaultSystemPrompt ?? base.defaultSystemPrompt,
		sizes: { ...base.sizes, ...override.sizes },
	};
}

function loadConfigFile(filePath: string): Config | undefined {
	if (!fs.existsSync(filePath)) return undefined;

	try {
		const parsed = JSON.parse(fs.readFileSync(filePath, "utf-8")) as RawConfig;
		return normalizeConfig(parsed);
	} catch {
		return undefined;
	}
}

function loadConfig(): Config {
	const globalDir = getAgentDir();
	const projectDir = path.join(process.cwd(), ".pi");
	const extensionDir = path.dirname(fileURLToPath(import.meta.url));
	const merged = mergeConfigs(
		loadConfigFile(path.join(extensionDir, "config.json")),
		loadConfigFile(path.join(globalDir, "subagent.json")),
	);
	const withGlobal = mergeConfigs(
		merged,
		loadConfigFile(path.join(globalDir, "subagent.config.json")),
	);
	const withProject = mergeConfigs(
		withGlobal,
		mergeConfigs(loadConfigFile(path.join(projectDir, "subagent.json")), loadConfigFile(path.join(projectDir, "subagent.config.json"))),
	);

	const envConfigPath = process.env.PI_SUBAGENT_CONFIG;
	if (!envConfigPath) return withProject;

	const envConfig = loadConfigFile(path.resolve(envConfigPath));
	return mergeConfigs(withProject, envConfig);
}

function getPiInvocation(command: string, args: string[]): { command: string; args: string[] } {
	if (command !== "pi") {
		return { command, args };
	}

	const currentScript = process.argv[1];
	const isBunVirtualScript = currentScript?.startsWith("/$bunfs/root/");
	if (currentScript && !isBunVirtualScript && fs.existsSync(currentScript)) {
		return { command: process.execPath, args: [currentScript, ...args] };
	}

	const execName = path.basename(process.execPath).toLowerCase();
	const isGenericRuntime = /^(node|bun)(\.exe)?$/.test(execName);
	if (!isGenericRuntime) {
		return { command: process.execPath, args };
	}

	return { command: "pi", args };
}

function getFinalAssistantText(messages: Array<{ role: string; content: Array<{ type: string; text?: string }> }>): string {
	for (let i = messages.length - 1; i >= 0; i--) {
		const message = messages[i];
		if (message.role !== "assistant") continue;
		for (const part of message.content) {
			if (part.type === "text" && typeof part.text === "string") return part.text;
		}
	}
	return "";
}

function consumeJsonEvents(buffer: string, onEvent: (event: unknown) => void): string {
	let start = -1;
	let depth = 0;
	let inString = false;
	let escape = false;

	for (let i = 0; i < buffer.length; i++) {
		const ch = buffer[i];

		if (start < 0) {
			if (ch === "{") {
				start = i;
				depth = 1;
				inString = false;
				escape = false;
			}
			continue;
		}

		if (inString) {
			if (escape) {
				escape = false;
			} else if (ch === "\\") {
				escape = true;
			} else if (ch === '"') {
				inString = false;
			}
			continue;
		}

		if (ch === '"') {
			inString = true;
		} else if (ch === "{") {
			depth++;
		} else if (ch === "}") {
			depth--;
			if (depth === 0) {
				const candidate = buffer.slice(start, i + 1);
				try {
					onEvent(JSON.parse(candidate));
				} catch {
					// Ignore malformed chunks and continue scanning for the next object.
				}
				start = -1;
			}
		}
	}

	return start >= 0 ? buffer.slice(start) : "";
}

function lastNonEmptyLines(text: string, count: number): string[] {
	return text
		.split(/\r?\n/)
		.map((line) => line.trimEnd())
		.filter((line) => line.trim().length > 0)
		.slice(-count);
}

function summarizeTask(task: string): string {
	const maxLength = 80;
	const singleLine = task.replace(/\s+/g, " ").trim();
	return singleLine.length <= maxLength ? singleLine : `${singleLine.slice(0, maxLength - 3)}...`;
}

const unicodeWordRe = /[\p{L}\p{N}]/u;
const unicodeUpperRe = /\p{Lu}/u;
const unicodeLowerRe = /\p{Ll}/u;

function estimateTokens(text: string): number {
	let count = 0;
	let open = false;
	let kind = 0; // 1 lower, 2 upper, 3 digit, 4 other word

	for (let i = 0; i < text.length; i++) {
		const code = text.charCodeAt(i);
		const ch = text[i];

		if (code <= 32 || ch === "_") {
			if (open) count++;
			open = false;
			kind = 0;
			continue;
		}

		let nextKind = 0;
		if (code >= 48 && code <= 57) nextKind = 3;
		else if (code >= 65 && code <= 90) nextKind = 2;
		else if (code >= 97 && code <= 122) nextKind = 1;
		else if (unicodeWordRe.test(ch)) nextKind = unicodeUpperRe.test(ch) ? 2 : unicodeLowerRe.test(ch) ? 1 : 4;

		if (nextKind === 0) {
			if (open) count++;
			count++;
			open = false;
			kind = 0;
			continue;
		}

		const next = text[i + 1];
		const nextLower = next !== undefined && ((next.charCodeAt(0) >= 97 && next.charCodeAt(0) <= 122) || unicodeLowerRe.test(next));

		if (open && ((kind === 1 && nextKind === 2) || (kind !== nextKind && (kind === 3 || nextKind === 3)) || (kind === 2 && nextKind === 2 && nextLower))) {
			count++;
		}

		open = true;
		kind = nextKind;
	}

	return count + (open ? 1 : 0);
}

async function killProcessTree(proc: ReturnType<typeof spawn>): Promise<void> {
	if (!proc.pid) return;

	if (process.platform === "win32") {
		spawnSync("taskkill", ["/pid", String(proc.pid), "/t", "/f"], { stdio: "ignore" });
		return;
	}

	try {
		process.kill(-proc.pid, "SIGTERM");
	} catch {
		try {
			proc.kill("SIGTERM");
		} catch {
			return;
		}
	}

	await new Promise((resolve) => setTimeout(resolve, 2000));
	if (proc.exitCode !== null || proc.signalCode !== null) return;

	try {
		process.kill(-proc.pid, "SIGKILL");
	} catch {
		try {
			proc.kill("SIGKILL");
		} catch {
			// The process exited between the status check and the signal.
		}
	}
}

async function writeSystemPrompt(systemPrompt: string): Promise<{ dir: string; filePath: string }> {
	const dir = await fs.promises.mkdtemp(path.join(os.tmpdir(), "pi-subagent-"));
	const filePath = path.join(dir, "system-prompt.md");
	await fs.promises.writeFile(filePath, systemPrompt, { encoding: "utf-8", mode: 0o600 });
	return { dir, filePath };
}

export default function (pi: ExtensionAPI) {
	pi.registerTool({
		name: "subagent",
		label: "Sub-agent",
		description: "Launch a delegated sub-agent using a size-to-model mapping from configuration.",
		parameters: Type.Object({
			modelSize: Type.Optional(Type.String({ description: "Model size key resolved through the subagent config." })),
			taskName: Type.Optional(Type.String({ description: "Short label for the sub-agent task." })),
			task: Type.String({ description: "Task for the sub-agent to execute." }),
			cwd: Type.Optional(Type.String({ description: "Working directory for the sub-agent process." })),
		}),
		renderResult(result, { isPartial }, theme, _context) {
			const details = result.details as {
				taskName?: string;
				taskSummary?: string;
				totalTokens?: number;
			} | undefined;
			const content = result.content[0];
			const output = content?.type === "text" ? content.text : "";
			const taskLabel = details?.taskName ?? "subagent";
			const summary = details?.taskSummary ?? "";
			const totalTokens = details?.totalTokens ?? 0;
			const headerParts = [
				theme.bg(
					"toolPendingBg",
					theme.fg("toolTitle", theme.bold(taskLabel)),
				),
			];
			if (summary) headerParts.push(theme.fg("accent", ` — ${summary}`));
			headerParts.push(theme.fg("dim", ` · ${totalTokens.toLocaleString()} tokens`));

			const lines = isPartial
				? [theme.fg("warning", "running...")]
				: lastNonEmptyLines(output, 2).map((line) => theme.fg("toolOutput", line));

			return new Text([headerParts.join(""), ...lines].join("\n"), 0, 0);
		},
		async execute(_toolCallId, params, signal, onUpdate) {
			const config = loadConfig();
			const sizeNames = Object.keys(config.sizes).sort();
			const taskName = params.taskName?.trim() || undefined;
			const taskSummary = summarizeTask(params.task);
			const requestedSize = params.modelSize ?? config.defaultSize;
			if (!requestedSize) {
				return {
					content: [{ type: "text", text: `No modelSize provided and no defaultSize configured. Available sizes: ${sizeNames.join(", ") || "none"}.` }],
					details: { mode: "subagent", taskName, taskSummary, modelSize: "", model: "", cwd: process.cwd(), exitCode: 1, usage: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0, cost: 0, contextTokens: 0, turns: 0 } },
				};
			}

			const sizeConfig = config.sizes[requestedSize];
			if (!sizeConfig) {
				return {
					content: [{ type: "text", text: `Unknown modelSize "${requestedSize}". Available sizes: ${sizeNames.join(", ") || "none"}.` }],
					details: { mode: "subagent", taskName, taskSummary, modelSize: requestedSize, model: "", cwd: process.cwd(), exitCode: 1, usage: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0, cost: 0, contextTokens: 0, turns: 0 } },
				};
			}

			const mergedTools = sizeConfig.tools ?? config.defaultTools;
			const taskLabel = taskName ?? requestedSize;
			const systemPrompt = [config.defaultSystemPrompt, sizeConfig.systemPrompt].filter(Boolean).join("\n\n");
			const cwd = params.cwd ?? process.cwd();
			const args = ["--mode", "json", "-p", "--no-session", "--model", sizeConfig.model];
			if (mergedTools && mergedTools.length > 0) {
				args.push("--tools", mergedTools.join(","));
			}

			let tempDir: string | undefined;
			if (systemPrompt.trim().length > 0) {
				const temp = await writeSystemPrompt(systemPrompt);
				tempDir = temp.dir;
				args.push("--append-system-prompt", temp.filePath);
			}

			args.push(`${taskName ? `Task name: ${taskName}\n\n` : ""}Task: ${params.task}`);

			const result = {
				messages: [] as Array<{ role: string; content: Array<{ type: string; text?: string }>; usage?: any; stopReason?: string; errorMessage?: string; model?: string }>,
				stderr: "",
				streamedText: "",
				firstTokenAtMs: undefined as number | undefined,
				stallTimeoutMs: Math.max(30_000, (config.stallTimeoutSeconds ?? 300) * 1000),
				lastActivityAtMs: Date.now(),
				watchdogTriggered: false,
				usage: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0, cost: 0, contextTokens: 0, turns: 0 },
				stopReason: undefined as string | undefined,
				errorMessage: undefined as string | undefined,
				model: sizeConfig.model,
			};

			const totalTokens = () => result.usage.contextTokens || result.usage.input + result.usage.output || estimateTokens(result.streamedText);

			const renderUpdate = () => {
				const header = `${taskLabel} — ${taskSummary} · ${totalTokens().toLocaleString()} tokens`;
				const lines = lastNonEmptyLines(result.streamedText || getFinalAssistantText(result.messages), 2);
				return [header, ...(lines.length > 0 ? lines : ["running..."])].join("\n");
			};

			const emitUpdate = () => {
				if (!onUpdate) return;
				onUpdate({
					content: [{ type: "text", text: renderUpdate() }],
					details: {
						mode: "subagent",
						taskName,
						taskSummary,
						totalTokens: totalTokens(),
						modelSize: requestedSize,
						model: result.model,
						cwd,
						exitCode: 0,
						stopReason: result.stopReason,
						errorMessage: result.errorMessage,
						usage: result.usage,
					},
				});
			};

			const markActivity = () => {
				result.lastActivityAtMs = Date.now();
			};

			let proc: ReturnType<typeof spawn> | undefined;
			let jsonBuffer = "";
			let watchdog: ReturnType<typeof setInterval> | undefined;
			let finished = false;
			let timeoutReason: string | undefined;
			let killListener: (() => void) | undefined;
			let aborted = false;

			try {
				const invocation = getPiInvocation(config.piCommand ?? "pi", args);
				proc = spawn(invocation.command, invocation.args, {
					cwd,
					detached: process.platform !== "win32",
					shell: false,
					stdio: ["ignore", "pipe", "pipe"],
				});
				emitUpdate();

				const processEvent = (event: any) => {
					if (event.type === "message_update" && event.assistantMessageEvent?.type === "text_delta") {
						const delta = event.assistantMessageEvent.delta;
						if (typeof delta === "string" && delta.length > 0) {
							result.firstTokenAtMs ??= Date.now();
							result.streamedText += delta;
							emitUpdate();
						}
					}

					if (event.type === "message_end" && event.message) {
						result.messages.push(event.message);
						if (event.message.role === "assistant") {
							const finalText = getFinalAssistantText([event.message]);
							if (!result.streamedText && finalText) {
								result.firstTokenAtMs ??= Date.now();
								result.streamedText = finalText;
							}
							const usage = event.message.usage;
							if (usage) {
								result.usage.input += usage.input || 0;
								result.usage.output += usage.output || 0;
								result.usage.cacheRead += usage.cacheRead || 0;
								result.usage.cacheWrite += usage.cacheWrite || 0;
								result.usage.cost += usage.cost?.total || 0;
								result.usage.contextTokens = usage.totalTokens || 0;
							}
							if (event.message.stopReason) result.stopReason = event.message.stopReason;
							if (event.message.errorMessage) result.errorMessage = event.message.errorMessage;
							if (event.message.model) result.model = event.message.model;
							result.usage.turns++;
						}
						emitUpdate();
					}

					if (event.type === "tool_result_end" && event.message) {
						result.messages.push(event.message);
						emitUpdate();
					}
				};

				const processJsonChunk = (chunk: string) => {
					if (!chunk) return;
					markActivity();
					jsonBuffer += chunk;
					jsonBuffer = consumeJsonEvents(jsonBuffer, processEvent);
				};

				proc.stdout.on("data", (data) => {
					processJsonChunk(data.toString());
				});

				proc.stderr.on("data", (data) => {
					markActivity();
					result.stderr += data.toString();
				});

				if (signal) {
					killListener = () => {
						aborted = true;
						if (proc) void killProcessTree(proc);
					};
					signal.addEventListener("abort", killListener, { once: true });
				}

				watchdog = setInterval(() => {
					if (finished || aborted || timeoutReason || !proc?.pid) return;
					if (Date.now() - result.lastActivityAtMs > result.stallTimeoutMs) {
						result.watchdogTriggered = true;
						timeoutReason = `Sub-agent stalled for ${Math.round(result.stallTimeoutMs / 1000)}s.`;
						result.errorMessage = timeoutReason;
						result.stopReason = "timeout";
						emitUpdate();
						if (proc) void killProcessTree(proc);
					}
				}, 5000);
				watchdog.unref?.();

				const exitCode = await new Promise<number>((resolve) => {
					const settle = (code: number) => {
						if (finished) return;
						finished = true;
						if (watchdog) clearInterval(watchdog);
						if (jsonBuffer.trim()) jsonBuffer = consumeJsonEvents(jsonBuffer, processEvent);
						resolve(code);
					};
					proc?.on("close", (code) => settle(code ?? 1));
					proc?.on("error", () => settle(1));
				});

				const effectiveExitCode = aborted || timeoutReason ? 1 : exitCode;
				const text =
					getFinalAssistantText(result.messages) ||
					(timeoutReason ?? result.errorMessage) ||
					result.stderr ||
					(aborted ? "(aborted)" : "(no output)");
				return {
					content: [{ type: "text", text }],
					details: {
						mode: "subagent",
						taskName,
						taskSummary,
						totalTokens: totalTokens(),
						modelSize: requestedSize,
						model: result.model,
						cwd,
						exitCode: effectiveExitCode,
						stopReason: aborted ? "aborted" : timeoutReason ? "timeout" : result.stopReason,
						errorMessage: aborted ? "Sub-agent was aborted." : timeoutReason ?? result.errorMessage,
						usage: result.usage,
					},
				};
			} finally {
				if (killListener && signal) {
					signal.removeEventListener("abort", killListener);
				}
				if (tempDir) {
					await fs.promises.rm(tempDir, { recursive: true, force: true }).catch(() => undefined);
				}
			}
		},
	});
}

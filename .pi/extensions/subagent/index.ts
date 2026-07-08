import { spawn } from "node:child_process";
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
	defaultTools?: string[];
	defaultSystemPrompt?: string;
	sizes?: Record<string, string | Partial<SizeConfig> & { id?: string }>;
	[size: string]: unknown;
};

type Config = {
	defaultSize?: string;
	piCommand?: string;
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
		loadConfigFile(path.join(globalDir, "subagent.json")),
		loadConfigFile(path.join(globalDir, "subagent.config.json")),
	);
	const withProject = mergeConfigs(
		merged,
		mergeConfigs(loadConfigFile(path.join(projectDir, "subagent.json")), loadConfigFile(path.join(projectDir, "subagent.config.json"))),
	);
	const withExtension = mergeConfigs(withProject, loadConfigFile(path.join(extensionDir, "config.json")));

	const envConfigPath = process.env.PI_SUBAGENT_CONFIG;
	if (!envConfigPath) return withExtension;

	const envConfig = loadConfigFile(path.resolve(envConfigPath));
	return mergeConfigs(withExtension, envConfig);
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

function estimateTokens(text: string): number {
	return Math.ceil(text.length / 4);
}

function formatSeconds(ms: number | undefined): string {
	if (ms === undefined) return "pending";
	return `${(ms / 1000).toFixed(1)}s`;
}

function formatTokenRate(tokensPerSecond: number): string {
	return `${tokensPerSecond.toFixed(1)} tok/s`;
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

			const startedAtMs = Date.now();
			const result = {
				messages: [] as Array<{ role: string; content: Array<{ type: string; text?: string }>; usage?: any; stopReason?: string; errorMessage?: string; model?: string }>,
				stderr: "",
				streamedText: "",
				firstTokenAtMs: undefined as number | undefined,
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

			let proc: ReturnType<typeof spawn> | undefined;
			let buffer = "";
			let killListener: (() => void) | undefined;
			let aborted = false;

			try {
				const invocation = getPiInvocation(config.piCommand ?? "pi", args);
				proc = spawn(invocation.command, invocation.args, {
					cwd,
					shell: false,
					stdio: ["ignore", "pipe", "pipe"],
				});
				emitUpdate();

				const processLine = (line: string) => {
					if (!line.trim()) return;
					let event: any;
					try {
						event = JSON.parse(line);
					} catch {
						return;
					}

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

				proc.stdout.on("data", (data) => {
					buffer += data.toString();
					const lines = buffer.split("\n");
					buffer = lines.pop() ?? "";
					for (const line of lines) processLine(line);
				});

				proc.stderr.on("data", (data) => {
					result.stderr += data.toString();
				});

				if (signal) {
					killListener = () => {
						aborted = true;
						proc?.kill();
					};
					signal.addEventListener("abort", killListener, { once: true });
				}

				const exitCode = await new Promise<number>((resolve) => {
					proc?.on("close", (code) => {
						if (buffer.trim()) processLine(buffer);
						resolve(code ?? 0);
					});
					proc?.on("error", () => resolve(1));
				});

				const effectiveExitCode = aborted ? 1 : exitCode;
				const text = getFinalAssistantText(result.messages) || result.errorMessage || result.stderr || (aborted ? "(aborted)" : "(no output)");
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
						stopReason: aborted ? "aborted" : result.stopReason,
						errorMessage: aborted ? "Sub-agent was aborted." : result.errorMessage,
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

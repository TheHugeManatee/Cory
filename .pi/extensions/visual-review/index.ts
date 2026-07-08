import { convertToPng, defineTool, type ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";

import * as fs from "node:fs/promises";
import * as path from "node:path";

type VisualReviewMetadata = {
	catchTestName?: string;
	sourceFile?: string;
	sourceLine?: number;
	sourceFunction?: string;
};

type VisualReviewMetrics = {
	mismatchedPixels?: number;
	mismatchRatio?: number;
	maxChannelError?: number;
	meanAbsoluteError?: number;
};

type VisualReviewRequest = {
	schema?: string;
	id: string;
	caseName: string;
	metadata?: VisualReviewMetadata;
	baselinePath: string;
	actualPath: string;
	diffPath: string;
	metricsPath?: string;
	requestPath?: string;
	decisionPath: string;
	metrics?: VisualReviewMetrics;
};

type VisualReviewDecision = {
	schema: string;
	requestId: string;
	accepted: boolean;
	note: string;
};

const visualReviewTool = defineTool({
	name: "visual_review_request",
	label: "Visual Review Request",
	description:
		"Inspect a Cory visual review request from request.json, including baseline/actual/diff images and source context. Optionally accept the change to update the baseline.",
	promptSnippet: "Inspect a Cory visual review request with images and source excerpt; optionally accept it if the change is intended.",
	promptGuidelines: [
		"Use visual_review_request when a Cory visual test produces a request.json mismatch artifact and you need to understand the regression.",
		"Call visual_review_request with action 'inspect' first so you can review the baseline image, actual image, diff image, and source-code context.",
		"Treat the attached images as ordered and explicitly labeled in the text response as IMAGE 1 = baseline, IMAGE 2 = actual, IMAGE 3 = diff.",
		"Use visual_review_request with action 'accept' only when the visual change is intended and the baseline should be updated to match the actual image.",
		"You usually do not need to call visual_review_request with action 'reject'; if the change is not intended, continue iterating on the code instead.",
	],
	parameters: Type.Object({
		action: Type.Union([
			Type.Literal("inspect"),
			Type.Literal("accept"),
			Type.Literal("reject"),
		]),
		requestPath: Type.String({ description: "Path to Cory visual review request.json" }),
		note: Type.Optional(
			Type.String({
				description:
					"Short rationale for accepting or rejecting. Recommended for decision actions; ignored for inspect if omitted.",
			}),
		),
		contextLines: Type.Optional(
			Type.Number({
				description: "Approximate number of source lines to show around the failing visual assertion",
				minimum: 5,
				maximum: 60,
			}),
		),
	}),
	async execute(_toolCallId, params) {
		try {
			const resolvedRequestPath = path.resolve(params.requestPath);
			const request = await readRequest(resolvedRequestPath);
			const requestDir = path.dirname(resolvedRequestPath);
			const baselinePath = resolveArtifactPath(requestDir, request.baselinePath);
			const actualPath = resolveArtifactPath(requestDir, request.actualPath);
			const diffPath = resolveArtifactPath(requestDir, request.diffPath);
			const decisionPath = resolveArtifactPath(requestDir, request.decisionPath);
			const sourceFile = request.metadata?.sourceFile
				? resolveArtifactPath(requestDir, request.metadata.sourceFile)
				: undefined;
			const sourceLine = request.metadata?.sourceLine ?? 0;
			const contextLines = clamp(Math.round(params.contextLines ?? 11), 5, 60);
			const sourceExcerpt = sourceFile
				? await readSourceExcerpt(sourceFile, sourceLine, contextLines)
				: "Source context unavailable.";
			const existingDecision = await readDecisionIfPresent(decisionPath);

			if (params.action === "inspect") {
				const baselineImage = await loadImageContent(baselinePath, "Baseline image");
				const actualImage = await loadImageContent(actualPath, "Actual image");
				const diffImage = await loadImageContent(diffPath, "Diff image");

				const summaryLines = [
					`Visual review request: ${request.id}`,
					`Case: ${request.caseName}`,
					request.metadata?.catchTestName ? `Catch test: ${request.metadata.catchTestName}` : undefined,
					sourceFile && sourceLine > 0 ? `Source: ${sourceFile}:${sourceLine}` : undefined,
					formatMetrics(request.metrics),
					existingDecision
						? `Existing decision: ${existingDecision.accepted ? "accepted" : "rejected"} (${existingDecision.note || "no note"})`
						: "Existing decision: none",
					"Attached image order: IMAGE 1 = baseline, IMAGE 2 = actual, IMAGE 3 = diff.",
					"Use this tool to inspect the regression. If the change is intended, you may follow up with action 'accept' to update the baseline.",
				]
					.filter((line): line is string => Boolean(line))
					.join("\n");

				return {
					content: [
						{ type: "text", text: summaryLines },
						{ type: "text", text: `Source excerpt:\n\n${sourceExcerpt}` },
						{ type: "text", text: "IMAGE 1 / 3 — BASELINE reference" },
						...(baselineImage ? [{ type: "image", data: baselineImage.data, mimeType: baselineImage.mimeType } as const] : []),
						{ type: "text", text: "IMAGE 2 / 3 — ACTUAL failed image" },
						...(actualImage ? [{ type: "image", data: actualImage.data, mimeType: actualImage.mimeType } as const] : []),
						{ type: "text", text: "IMAGE 3 / 3 — DIFF image" },
						...(diffImage ? [{ type: "image", data: diffImage.data, mimeType: diffImage.mimeType } as const] : []),
					],
					details: {
						request,
						baselinePath,
						actualPath,
						diffPath,
						decisionPath,
						sourceExcerpt,
						existingDecision,
					},
				};
			}

			const accepted = params.action === "accept";
			const note = params.note?.trim() || (accepted ? "accepted by model review" : "rejected by model review");
			if (accepted) {
				await fs.copyFile(actualPath, baselinePath);
			}

			const decision: VisualReviewDecision = {
				schema: "cory.visual-review-decision.v1",
				requestId: request.id,
				accepted,
				note,
			};
			await fs.mkdir(path.dirname(decisionPath), { recursive: true });
			await fs.writeFile(decisionPath, `${JSON.stringify(decision, null, 2)}\n`, "utf-8");

			return {
				content: [
					{
						type: "text",
						text: accepted
							? `Accepted visual review request ${request.id}. Baseline updated from actual image. Note: ${note}`
							: `Rejected visual review request ${request.id}. Baseline left unchanged. Note: ${note}`,
					},
				],
				details: {
					requestId: request.id,
					accepted,
					note,
					baselinePath,
					actualPath,
					decisionPath,
				},
				terminate: true,
			};
		} catch (error) {
			const message = error instanceof Error ? error.message : String(error);
			return {
				content: [{ type: "text", text: `visual_review_request failed: ${message}` }],
				details: { error: message },
				isError: true,
			};
		}
	},
});

function clamp(value: number, min: number, max: number): number {
	return Math.min(max, Math.max(min, value));
}

async function readRequest(requestPath: string): Promise<VisualReviewRequest> {
	const raw = await fs.readFile(requestPath, "utf-8");
	const parsed = JSON.parse(raw) as Partial<VisualReviewRequest>;
	if (!parsed.id || !parsed.caseName || !parsed.baselinePath || !parsed.actualPath || !parsed.diffPath || !parsed.decisionPath) {
		throw new Error(`Invalid visual review request: ${requestPath}`);
	}
	return parsed as VisualReviewRequest;
}

async function readDecisionIfPresent(decisionPath: string): Promise<VisualReviewDecision | undefined> {
	try {
		const raw = await fs.readFile(decisionPath, "utf-8");
		return JSON.parse(raw) as VisualReviewDecision;
	} catch {
		return undefined;
	}
}

function resolveArtifactPath(baseDir: string, candidate: string): string {
	return path.isAbsolute(candidate) ? path.normalize(candidate) : path.resolve(baseDir, candidate);
}

function formatMetrics(metrics: VisualReviewMetrics | undefined): string {
	if (!metrics) return "Metrics: unavailable";
	return `Metrics: mismatchedPixels=${metrics.mismatchedPixels ?? 0}, mismatchRatio=${metrics.mismatchRatio ?? 0}, maxChannelError=${metrics.maxChannelError ?? 0}, meanAbsoluteError=${metrics.meanAbsoluteError ?? 0}`;
}

async function readSourceExcerpt(sourceFile: string, sourceLine: number, contextLines: number): Promise<string> {
	try {
		const raw = await fs.readFile(sourceFile, "utf-8");
		const lines = raw.split(/\r?\n/);
		if (lines.length === 0) return `Source file is empty: ${sourceFile}`;
		const center = sourceLine > 0 ? sourceLine - 1 : 0;
		const before = Math.max(0, Math.min(7, contextLines - 1));
		const after = Math.max(0, Math.min(3, contextLines - 1 - before));
		const start = Math.max(0, center - before);
		const end = Math.min(lines.length, center + after + 1);
		return lines
			.slice(start, end)
			.map((line, index) => {
				const lineNumber = start + index + 1;
				const marker = lineNumber === sourceLine ? ">" : " ";
				return `${marker} ${String(lineNumber).padStart(4, " ")} | ${line}`;
			})
			.join("\n");
	} catch (error) {
		const message = error instanceof Error ? error.message : String(error);
		return `Could not read source excerpt from ${sourceFile}: ${message}`;
	}
}

async function loadImageContent(imagePath: string, label: string): Promise<{ data: string; mimeType: string } | undefined> {
	try {
		const bytes = await fs.readFile(imagePath);
		const base64 = Buffer.from(bytes).toString("base64");
		const mimeType = mimeTypeFromPath(imagePath);
		const png = await convertToPng(base64, mimeType);
		if (png) return png;
		return { data: base64, mimeType };
	} catch {
		void label;
		return undefined;
	}
}

function mimeTypeFromPath(filePath: string): string {
	const ext = path.extname(filePath).toLowerCase();
	if (ext === ".png") return "image/png";
	if (ext === ".jpg" || ext === ".jpeg") return "image/jpeg";
	if (ext === ".webp") return "image/webp";
	if (ext === ".gif") return "image/gif";
	if (ext === ".bmp") return "image/bmp";
	return "application/octet-stream";
}

export default function visualReviewExtension(pi: ExtensionAPI) {
	pi.registerTool(visualReviewTool);
}

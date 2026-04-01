import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
	return twMerge(clsx(inputs));
}

export function formatUnit(unit: string): string {
	return /^[°%]/.test(unit) ? unit : ` ${unit}`;
}

export function utcToLocal(hhmm: string): string {
	const match = /^(\d{1,2}):(\d{2})$/.exec(hhmm);
	if (!match) return hhmm;
	const utcMinutes = Number(match[1]) * 60 + Number(match[2]);
	const offsetMinutes = -new Date().getTimezoneOffset();
	const localMinutes = ((utcMinutes + offsetMinutes) % 1440 + 1440) % 1440;
	return `${String(Math.floor(localMinutes / 60)).padStart(2, "0")}:${String(localMinutes % 60).padStart(2, "0")}`;
}

export function localToUtc(hhmm: string): string {
	const match = /^(\d{1,2}):(\d{2})$/.exec(hhmm);
	if (!match) return hhmm;
	const localMinutes = Number(match[1]) * 60 + Number(match[2]);
	const offsetMinutes = -new Date().getTimezoneOffset();
	const utcMinutes = ((localMinutes - offsetMinutes) % 1440 + 1440) % 1440;
	return `${String(Math.floor(utcMinutes / 60)).padStart(2, "0")}:${String(utcMinutes % 60).padStart(2, "0")}`;
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type WithoutChild<T> = T extends { child?: any } ? Omit<T, "child"> : T;
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type WithoutChildren<T> = T extends { children?: any } ? Omit<T, "children"> : T;
export type WithoutChildrenOrChild<T> = WithoutChildren<WithoutChild<T>>;
export type WithElementRef<T, U extends HTMLElement = HTMLElement> = T & { ref?: U | null };

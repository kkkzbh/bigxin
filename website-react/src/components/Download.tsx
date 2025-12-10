import { motion } from "framer-motion";
import { Monitor, Apple, Terminal } from "lucide-react";

const DRIVE_LINK =
  "https://drive.google.com/uc?export=download&id=1kvOSl1XaUEH4JSHicU7bR872RU5Vfmr7";

const platforms = [
  {
    name: "Windows",
    icon: Monitor,
    available: true,
    href: DRIVE_LINK,
    badge: "立即下载",
  },
  {
    name: "Linux",
    icon: Terminal,
    available: false,
    href: "#",
    badge: "即将推出",
  },
  {
    name: "macOS",
    icon: Apple,
    available: false,
    href: "#",
    badge: "即将推出",
  },
];

export default function Download() {
  return (
    <section
      id="download"
      className="relative py-28 px-6 bg-gradient-to-b from-[#0d0f18] via-[#0b0d14] to-[#0b0d14]"
    >
      <div className="max-w-5xl mx-auto">
        <div className="relative rounded-[2rem] overflow-hidden border border-indigo-500/30 bg-gradient-to-br from-indigo-900/30 via-[#0f1020] to-purple-900/20 p-10 md:p-16 shadow-2xl">
          {/* Decorative blobs */}
          <div className="pointer-events-none absolute -top-20 -left-20 w-72 h-72 rounded-full bg-indigo-600/20 blur-3xl" />
          <div className="pointer-events-none absolute -bottom-20 -right-20 w-64 h-64 rounded-full bg-purple-600/20 blur-3xl" />

          <motion.p
            initial={{ opacity: 0 }}
            whileInView={{ opacity: 1 }}
            viewport={{ once: true }}
            className="relative uppercase tracking-[0.25em] text-xs text-indigo-300"
          >
            立即上手
          </motion.p>
          <motion.h2
            initial={{ opacity: 0, y: 20 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true }}
            className="relative mt-3 text-3xl md:text-5xl font-bold"
          >
            下载聚信桌面端
          </motion.h2>
          <motion.p
            initial={{ opacity: 0 }}
            whileInView={{ opacity: 1 }}
            viewport={{ once: true }}
            className="relative mt-4 text-gray-400 max-w-lg"
          >
            Windows 版现已开放，Linux 与 macOS 版即将推出。快速安装，自动连接服务器，开始享受流畅的即时通讯体验。
          </motion.p>

          {/* Platform cards */}
          <div className="relative mt-12 grid gap-6 sm:grid-cols-3">
            {platforms.map((p, i) => (
              <motion.a
                key={i}
                href={p.available ? p.href : undefined}
                target={p.available ? "_blank" : undefined}
                rel={p.available ? "noreferrer noopener" : undefined}
                initial={{ opacity: 0, y: 30 }}
                whileInView={{ opacity: 1, y: 0 }}
                viewport={{ once: true }}
                transition={{ delay: i * 0.1 }}
                whileHover={p.available ? { y: -6, scale: 1.03 } : {}}
                className={`group flex flex-col items-center p-8 rounded-2xl border backdrop-blur-sm transition ${
                  p.available
                    ? "border-indigo-500/40 bg-indigo-500/10 hover:shadow-indigo-500/30 hover:shadow-xl cursor-pointer"
                    : "border-white/10 bg-white/[0.02] opacity-60 cursor-not-allowed"
                }`}
              >
                <p.icon
                  size={48}
                  className={`${
                    p.available ? "text-indigo-400" : "text-gray-500"
                  }`}
                />
                <span className="mt-4 text-xl font-semibold">{p.name}</span>
                <span
                  className={`mt-2 text-sm px-3 py-1 rounded-full ${
                    p.available
                      ? "bg-green-500/20 text-green-300"
                      : "bg-yellow-500/20 text-yellow-300"
                  }`}
                >
                  {p.badge}
                </span>
              </motion.a>
            ))}
          </div>

          <p className="relative mt-10 text-center text-gray-500 text-sm">
            文件托管于 Google Drive · 如遇限速可稍后重试或联系开发者获取备用链接
          </p>
        </div>
      </div>
    </section>
  );
}

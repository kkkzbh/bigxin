import { motion } from "framer-motion";
import { ChevronDown } from "lucide-react";

interface HeroProps {
  onDownloadClick: () => void;
}

export default function Hero({ onDownloadClick }: HeroProps) {
  return (
    <section className="relative min-h-screen flex flex-col items-center justify-center px-6 pt-24 pb-12 overflow-hidden">
      {/* Floating decorative elements */}
      <motion.div
        animate={{ y: [0, -20, 0] }}
        transition={{ duration: 6, repeat: Infinity, ease: "easeInOut" }}
        className="absolute top-1/4 left-[10%] w-20 h-20 rounded-2xl bg-gradient-to-br from-indigo-500/30 to-purple-500/20 blur-xl"
      />
      <motion.div
        animate={{ y: [0, 20, 0] }}
        transition={{ duration: 5, repeat: Infinity, ease: "easeInOut" }}
        className="absolute bottom-1/4 right-[12%] w-28 h-28 rounded-full bg-gradient-to-tr from-cyan-500/25 to-teal-400/15 blur-xl"
      />

      <motion.div
        initial={{ opacity: 0, y: 40 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ duration: 0.8 }}
        className="text-center max-w-4xl"
      >
        <p className="uppercase tracking-[0.3em] text-xs md:text-sm text-indigo-300 mb-4">
          跨平台 · 实时通信 · AI 助手
        </p>
        <h1 className="text-4xl md:text-6xl lg:text-7xl font-extrabold leading-tight bg-gradient-to-r from-white via-indigo-100 to-purple-200 bg-clip-text text-transparent">
          让沟通更高效
          <br />
          团队与好友都能用
        </h1>
        <p className="mt-6 text-base md:text-lg text-gray-400 max-w-2xl mx-auto">
          聚信是为桌面用户打造的现代化 IM 客户端，支持群聊、私聊、好友系统、历史记录与
          AI 智能回复。轻盈界面、可靠传输，让每一次交流更顺畅。
        </p>

        {/* CTA buttons */}
        <div className="mt-10 flex flex-col sm:flex-row items-center justify-center gap-4">
          <motion.button
            whileHover={{ scale: 1.05 }}
            whileTap={{ scale: 0.95 }}
            onClick={onDownloadClick}
            className="px-8 py-4 rounded-full bg-gradient-to-r from-indigo-600 to-purple-600 text-white font-semibold text-lg shadow-xl hover:shadow-indigo-500/50 transition"
          >
            下载 Windows · 聚信 1.0
          </motion.button>
          <motion.a
            whileHover={{ scale: 1.03 }}
            href="#features"
            className="px-8 py-4 rounded-full border border-white/20 text-white/80 hover:bg-white/5 transition"
          >
            了解更多
          </motion.a>
        </div>

        {/* Platform badges */}
        <div className="mt-8 flex flex-wrap justify-center gap-3 text-sm">
          <span className="px-4 py-1.5 rounded-full border border-green-400/40 bg-green-400/10 text-green-300">
            ✓ Windows
          </span>
          <span className="px-4 py-1.5 rounded-full border border-yellow-400/40 bg-yellow-400/10 text-yellow-300">
            Linux 即将推出
          </span>
          <span className="px-4 py-1.5 rounded-full border border-yellow-400/40 bg-yellow-400/10 text-yellow-300">
            macOS 即将推出
          </span>
        </div>
      </motion.div>

      {/* Hero image with parallax */}
      <motion.div
        initial={{ opacity: 0, y: 60 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ duration: 1, delay: 0.3 }}
        className="mt-16 w-full max-w-5xl px-4"
      >
        <div className="relative rounded-3xl overflow-hidden border border-white/10 bg-gradient-to-b from-white/5 to-transparent p-2 shadow-2xl">
          <img
            src="/screenshots/主界面.png"
            alt="聚信主界面"
            className="w-full rounded-2xl"
          />
          {/* Glare overlay */}
          <div className="absolute inset-0 bg-gradient-to-br from-white/10 via-transparent to-transparent pointer-events-none rounded-2xl" />
        </div>
      </motion.div>

      {/* Scroll indicator */}
      <motion.div
        animate={{ y: [0, 12, 0] }}
        transition={{ duration: 2, repeat: Infinity }}
        className="absolute bottom-8 text-gray-500"
      >
        <ChevronDown size={28} />
      </motion.div>
    </section>
  );
}

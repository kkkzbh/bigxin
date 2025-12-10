import { useState } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { ChevronLeft, ChevronRight, X } from "lucide-react";

const screenshots = [
  { src: "/screenshots/登录.png", label: "登录与账号入口" },
  { src: "/screenshots/主界面.png", label: "主界面导航" },
  { src: "/screenshots/好友列表.png", label: "好友与联系人" },
  { src: "/screenshots/群组管理.png", label: "群组创建与管理" },
  { src: "/screenshots/搜索栏.png", label: "搜索与快速定位" },
  { src: "/screenshots/AI帮写.png", label: "AI 助手生成回复" },
];

export default function Screenshots() {
  const [active, setActive] = useState(0);
  const [lightbox, setLightbox] = useState<number | null>(null);

  const prev = () =>
    setActive((a) => (a === 0 ? screenshots.length - 1 : a - 1));
  const next = () =>
    setActive((a) => (a === screenshots.length - 1 ? 0 : a + 1));

  return (
    <section
      id="screenshots"
      className="relative py-28 px-6 bg-gradient-to-b from-[#0d0f18] via-[#0b0d14] to-[#0d0f18]"
    >
      <div className="max-w-6xl mx-auto text-center">
        <motion.p
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          className="uppercase tracking-[0.25em] text-xs text-indigo-300"
        >
          界面预览
        </motion.p>
        <motion.h2
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="mt-3 text-3xl md:text-5xl font-bold"
        >
          现代化 UI，开箱即用
        </motion.h2>
        <motion.p
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          className="mt-4 text-gray-400 max-w-xl mx-auto"
        >
          涵盖登录、聊天、群组管理、AI 助手等关键界面。
        </motion.p>
      </div>

      {/* Main Carousel */}
      <div className="relative mt-14 max-w-5xl mx-auto">
        <motion.div
          key={active}
          initial={{ opacity: 0, scale: 0.95 }}
          animate={{ opacity: 1, scale: 1 }}
          transition={{ duration: 0.4 }}
          className="relative rounded-3xl overflow-hidden border border-white/10 bg-white/[0.02] p-2 shadow-2xl cursor-pointer"
          onClick={() => setLightbox(active)}
        >
          <img
            src={screenshots[active].src}
            alt={screenshots[active].label}
            className="w-full rounded-2xl"
          />
          <div className="absolute bottom-4 left-4 px-4 py-2 rounded-full bg-black/60 backdrop-blur text-sm">
            {screenshots[active].label}
          </div>
        </motion.div>

        {/* Arrows */}
        <button
          onClick={prev}
          className="absolute left-2 top-1/2 -translate-y-1/2 w-12 h-12 flex items-center justify-center rounded-full bg-black/50 hover:bg-black/70 transition"
          aria-label="上一张"
        >
          <ChevronLeft size={24} />
        </button>
        <button
          onClick={next}
          className="absolute right-2 top-1/2 -translate-y-1/2 w-12 h-12 flex items-center justify-center rounded-full bg-black/50 hover:bg-black/70 transition"
          aria-label="下一张"
        >
          <ChevronRight size={24} />
        </button>
      </div>

      {/* Thumbnails */}
      <div className="mt-8 flex justify-center gap-3 flex-wrap">
        {screenshots.map((s, i) => (
          <button
            key={i}
            onClick={() => setActive(i)}
            className={`w-20 h-14 rounded-lg overflow-hidden border-2 transition ${
              i === active
                ? "border-indigo-500 scale-105"
                : "border-transparent opacity-60 hover:opacity-100"
            }`}
          >
            <img
              src={s.src}
              alt={s.label}
              className="w-full h-full object-cover"
            />
          </button>
        ))}
      </div>

      {/* Lightbox */}
      <AnimatePresence>
        {lightbox !== null && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            className="fixed inset-0 z-50 flex items-center justify-center bg-black/90 backdrop-blur-sm p-4"
            onClick={() => setLightbox(null)}
          >
            <button
              className="absolute top-6 right-6 w-10 h-10 flex items-center justify-center rounded-full bg-white/10 hover:bg-white/20 transition"
              aria-label="关闭"
            >
              <X size={24} />
            </button>
            <motion.img
              initial={{ scale: 0.9 }}
              animate={{ scale: 1 }}
              exit={{ scale: 0.9 }}
              src={screenshots[lightbox].src}
              alt={screenshots[lightbox].label}
              className="max-w-full max-h-[90vh] rounded-2xl shadow-2xl"
              onClick={(e) => e.stopPropagation()}
            />
          </motion.div>
        )}
      </AnimatePresence>
    </section>
  );
}

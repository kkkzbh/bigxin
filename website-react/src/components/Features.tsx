import { motion } from "framer-motion";
import { MessageCircle, Users, BotMessageSquare, Zap } from "lucide-react";

const features = [
  {
    icon: MessageCircle,
    title: "群聊 & 私聊",
    desc: "创建群组、加入讨论、私聊与好友消息，一切实时送达。",
    color: "from-indigo-500 to-purple-500",
  },
  {
    icon: Users,
    title: "好友与资料",
    desc: "添加/搜索好友、处理请求、修改昵称与头像，资料同步更轻松。",
    color: "from-pink-500 to-rose-500",
  },
  {
    icon: BotMessageSquare,
    title: "AI 智能助手",
    desc: "集成 AI 回复，快速生成草稿与灵感，提升对话效率。",
    color: "from-cyan-500 to-teal-500",
  },
  {
    icon: Zap,
    title: "稳定低延迟",
    desc: "自研协议 + 本地头像缓存，消息推送与历史记录拉取更迅速。",
    color: "from-amber-500 to-orange-500",
  },
];

const containerVariants = {
  hidden: {},
  visible: {
    transition: { staggerChildren: 0.15 },
  },
};

const cardVariants = {
  hidden: { opacity: 0, y: 40 },
  visible: { opacity: 1, y: 0, transition: { duration: 0.6, ease: [0.25, 0.1, 0.25, 1] as const } },
};

export default function Features() {
  return (
    <section
      id="features"
      className="relative py-28 px-6 bg-gradient-to-b from-transparent via-[#0d0f18] to-transparent"
    >
      <div className="max-w-6xl mx-auto text-center">
        <motion.p
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          className="uppercase tracking-[0.25em] text-xs text-indigo-300"
        >
          核心特性
        </motion.p>
        <motion.h2
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          transition={{ delay: 0.1 }}
          className="mt-3 text-3xl md:text-5xl font-bold"
        >
          为团队与好友设计的高效沟通体验
        </motion.h2>
        <motion.p
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          transition={{ delay: 0.2 }}
          className="mt-4 text-gray-400 max-w-xl mx-auto"
        >
          即时稳定、界面现代、功能齐备，开箱即用。
        </motion.p>
      </div>

      <motion.div
        variants={containerVariants}
        initial="hidden"
        whileInView="visible"
        viewport={{ once: true, amount: 0.3 }}
        className="mt-16 max-w-6xl mx-auto grid gap-6 sm:grid-cols-2 lg:grid-cols-4"
      >
        {features.map((f, i) => (
          <motion.article
            key={i}
            variants={cardVariants}
            whileHover={{ y: -8, scale: 1.02 }}
            className="group relative p-6 rounded-3xl border border-white/10 bg-white/[0.02] backdrop-blur-sm overflow-hidden"
          >
            {/* Gradient glow on hover */}
            <div
              className={`absolute -inset-1 rounded-3xl bg-gradient-to-br ${f.color} opacity-0 group-hover:opacity-20 blur-xl transition-opacity duration-500`}
            />
            <div
              className={`relative z-10 w-14 h-14 flex items-center justify-center rounded-2xl bg-gradient-to-br ${f.color} shadow-lg`}
            >
              <f.icon size={28} className="text-white" />
            </div>
            <h3 className="relative z-10 mt-5 text-xl font-semibold">
              {f.title}
            </h3>
            <p className="relative z-10 mt-2 text-gray-400 text-sm leading-relaxed">
              {f.desc}
            </p>
          </motion.article>
        ))}
      </motion.div>
    </section>
  );
}

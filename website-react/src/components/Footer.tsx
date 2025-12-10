export default function Footer() {
  return (
    <footer className="mt-auto border-t border-white/10 bg-[#08090f]">
      <div className="max-w-6xl mx-auto px-6 py-12 flex flex-col md:flex-row items-center justify-between gap-6">
        <div className="flex items-center gap-3">
          <img
            src="/app_icon.ico"
            alt="聚信图标"
            className="w-10 h-10 rounded-xl"
          />
          <div>
            <div className="font-bold">聚信 JuXin</div>
            <div className="text-xs text-gray-500">
              跨平台即时通讯 · 现代化体验
            </div>
          </div>
        </div>

        <div className="flex gap-6 text-sm text-gray-400">
          <a href="#features" className="hover:text-white transition">
            功能
          </a>
          <a href="#screenshots" className="hover:text-white transition">
            界面
          </a>
          <a href="#download" className="hover:text-white transition">
            下载
          </a>
        </div>

        <div className="text-xs text-gray-600">
          Copyright © {new Date().getFullYear()} 聚信
        </div>
      </div>
    </footer>
  );
}

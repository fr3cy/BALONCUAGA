#pragma once
static void statsPage(){
    using namespace MovementStats;
    card("Stats",{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,7});
    auto seconds=(GetTickCount64()-periodStarted)/1000;
    ImGui::TextDisabled(L(u8"Период: %llu:%02llu","Period: %llu:%02llu"),static_cast<unsigned long long>(seconds/60),static_cast<unsigned long long>(seconds%60));
    static int group=0;
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##Stats group",&group,"Bhop\0Duck / SGS\0");
    const auto &d=group?totals.duck:totals.bhop;
    ImGui::Text(L(u8"Всего: %llu","Total: %llu"),static_cast<unsigned long long>(d.total));
    if(ImGui::BeginTable("FOG distribution",3,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_RowBg)){
        ImGui::TableSetupColumn("FOG",ImGuiTableColumnFlags_WidthStretch,1);
        ImGui::TableSetupColumn(L(u8"Количество","Count"),ImGuiTableColumnFlags_WidthStretch,1.4f);
        ImGui::TableSetupColumn(L(u8"Доля","Share"),ImGuiTableColumnFlags_WidthStretch,1);
        ImGui::TableHeadersRow();
        for(int i=0;i<7;++i){
            ImGui::TableNextRow();ImGui::TableNextColumn();
            ImVec4 color=i>=1&&i<=2?ImVec4(.53f,.86f,.62f,1):ImVec4(.96f,.36f,.41f,1);
            if(i==0||i==6)color=ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            ImGui::TextColored(color,i==6?"6+":"%d",i);
            ImGui::TableNextColumn();ImGui::Text("%llu",static_cast<unsigned long long>(d.fog[i]));
            ImGui::TableNextColumn();ImGui::Text("%.2f%%",percent(d.fog[i],d.total));
        }ImGui::EndTable();
    }
    if(!group){
        auto metric=[](const char *name,Count count){ImGui::Text("%s: %llu (%.2f%%)",name,static_cast<unsigned long long>(count),percent(count,totals.bhop.total));};
        ImGui::Separator();metric("Ideal bhops",totals.ideal);metric("Ideal distr.",totals.distributed);metric("Jumpbug",totals.jumpbug);
    }
    ImGui::Separator();
    if(ImGui::Button(L(u8"Сбросить всю статистику","Reset all statistics"),{-1,27}))clear();
    ImGui::PopStyleVar();endCard();
}
